#include <stdio.h>
#include <dlfcn.h>

#import <AVFoundation/AVFoundation.h>

typedef void (*t_VTApplyRestrictions)(int arg);
t_VTApplyRestrictions VTApplyRestrictions;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#define FUZZ_TARGET_MODIFIERS __declspec(dllexport)
#else
#define FUZZ_TARGET_MODIFIERS __attribute__ ((noinline))
#endif

int FUZZ_TARGET_MODIFIERS fuzz(const char *filename) {
  @autoreleasepool {
    NSError *error = nil;
    NSURL *fileURL = [NSURL fileURLWithPath:[NSString stringWithCString:filename encoding:NSASCIIStringEncoding]];
    AVAsset *asset = [AVAsset assetWithURL:fileURL];
    if(asset == nil) return 0;
    
    AVAssetReader *reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
    if(reader == nil) return 0;
    
    NSArray *tracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    if(tracks == nil || ([tracks count] == 0)) return 0;
    
    AVAssetTrack *track = tracks[0];
    
    NSDictionary *outputSettings = [NSDictionary dictionaryWithObject:
                                    [NSNumber numberWithInt:kCMPixelFormat_32BGRA] forKey:(id)kCVPixelBufferPixelFormatTypeKey];
    AVAssetReaderTrackOutput *output = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track outputSettings:outputSettings];
    
    [reader addOutput:output];
    [reader startReading];
    
    for(int frame=0; frame < 2; frame++) {
      // printf("Frame %d\n", frame);
      
      CMSampleBufferRef sampleBuffer = [output copyNextSampleBuffer];
      if(sampleBuffer == nil) break;
           
      CMSampleBufferInvalidate(sampleBuffer);
      CFRelease(sampleBuffer);
      sampleBuffer = NULL;
    }
  }
  
  return 1;
}

int main(int argc, const char * argv[]) {
  if(argc < 2) {
    printf("Usage: %s <filename>\n", argv[0]);
    return 0;
  }
  
  // make decoding run in the current process instead of VTDecoderXPCService
  void *toolbox = dlopen("/System/Library/Frameworks/VideoToolbox.framework/Versions/A/VideoToolbox", RTLD_NOW);
  if(!toolbox) {
    printf("Error loading library\n");
    return 0;
  }
  VTApplyRestrictions = (t_VTApplyRestrictions)dlsym(toolbox, "VTApplyRestrictions");
  if(!VTApplyRestrictions) {
    printf("Error finding VTApplyRestrictions symbol\n");
    return 0;
  }
  VTApplyRestrictions(1);

  fuzz(argv[1]);
  return 0;
}
