#include <dlfcn.h>
#include <stdio.h>

#import <AVFoundation/AVFoundation.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#define FUZZ_TARGET_MODIFIERS __declspec(dllexport)
#else
#define FUZZ_TARGET_MODIFIERS __attribute__((noinline))
#endif

int FUZZ_TARGET_MODIFIERS fuzz(const char *filename) {
  @autoreleasepool {
    NSError *error = nil;
    NSURL *fileURL = [NSURL fileURLWithPath:[NSString stringWithCString:filename
                                                               encoding:NSASCIIStringEncoding]];
    AVAsset *asset = [AVAsset assetWithURL:fileURL];
    if (asset == nil) return 0;

    AVAssetReader *reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
    if (reader == nil) return 0;

    NSArray *tracks = [asset tracksWithMediaType:AVMediaTypeAudio];
    if (tracks == nil || ([tracks count] == 0)) return 0;
    for (int i = 0; i < tracks.count; i++) {
      AVAssetTrack *track = tracks[i];
      AVAssetReader *assetReader = [AVAssetReader assetReaderWithAsset:asset error:&error];
      if (error) {
        NSLog(@"Error creating AVAssetReader: %@", error.localizedDescription);
        return -1;
      }

      // Configure the output settings (PCM 16-bit, stereo, 44.1kHz)
      NSDictionary *outputSettings = @{
        AVFormatIDKey : @(kAudioFormatLinearPCM),
        AVSampleRateKey : @44100,
        AVNumberOfChannelsKey : @2,
        AVLinearPCMBitDepthKey : @16,
        AVLinearPCMIsNonInterleaved : @NO,
        AVLinearPCMIsFloatKey : @NO,
        AVLinearPCMIsBigEndianKey : @NO
      };

      AVAssetReaderTrackOutput *trackOutput =
          [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track
                                                     outputSettings:outputSettings];
      [assetReader addOutput:trackOutput];

      if (![assetReader startReading]) {
        NSLog(@"Error starting asset reader: %@", assetReader.error.localizedDescription);
        return -1;
      }

      CMSampleBufferRef sampleBuffer;
      while ((sampleBuffer = [trackOutput copyNextSampleBuffer])) {
        CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
        if (blockBuffer) {
          size_t length;
          char *data;
          OSStatus status = CMBlockBufferGetDataPointer(blockBuffer, 0, NULL, &length, &data);
          if (status == kCMBlockBufferNoErr) {
            NSLog(@"Read %zu bytes of audio data", length);
          } else {
            NSLog(@"Error accessing block buffer data");
          }
        }

        CFRelease(sampleBuffer);
      }

      // Check the reader status
      if (assetReader.status == AVAssetReaderStatusCompleted) {
        NSLog(@"Audio decoding completed successfully.");
      } else {
        NSLog(@"Error reading audio: %@", assetReader.error.localizedDescription);
      }
    }
  }

  return 1;
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <filename>\n", argv[0]);
    return 0;
  }
  fuzz(argv[1]);
  return 0;
}
