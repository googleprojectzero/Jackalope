/*
Copyright 2025 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "sancovclient.h"

// shared memory stuff
#include <sys/mman.h>

#define MAX_SAMPLE_SIZE 1000000
#define SHM_SIZE (4 + MAX_SAMPLE_SIZE)
unsigned char *shm_data;

bool use_shared_memory;

int setup_shmem(const char *name)
{
  int fd;

  // get shared memory file descriptor (NOT a file)
  fd = shm_open(name, O_RDONLY, S_IRUSR | S_IWUSR);
  if (fd == -1)
  {
    printf("Error in shm_open\n");
    return 0;
  }

  // map shared memory to process address space
  shm_data = (unsigned char *)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
  if (shm_data == MAP_FAILED)
  {
    printf("Error in mmap\n");
    return 0;
  }

  return 1;
}

// actual target function

#include "libxml/tree.h"
#include "libxml/parserInternals.h"
#include "libxslt/xsltutils.h"
#include "libxslt/transform.h"
#include "libxslt/security.h"
#include "libxslt/documents.h"

#define DELIMITER1 "<!delimiter1!>"
#define DELIMITER2 "<!delimiter2!>"

void delimiter_split(char *inp, const char *delimiter, char ***_split, size_t *_nsplit) {
  size_t nsplit = 1;
  char **split;
  char *p, *d;

  p = inp;
  do {
    d = strstr(p, delimiter);
    if(d) {
      nsplit++;
      p = d + strlen(delimiter);
    }
  } while(d);
  
  split = (char **)malloc(nsplit * sizeof(char *));
  
  size_t i = 0;
  p = inp;
  do {
    d = strstr(p, delimiter);
    if(d) {
      size_t size = (size_t)(d - p);
      split[i] = (char *)malloc(size + 1);
      memcpy(split[i], p, size);
      split[i][size] = 0;
      
      p = d + strlen(delimiter);
      i++;
    } else {
      size_t size = strlen(p);
      split[i] = (char *)malloc(size + 1);
      memcpy(split[i], p, size);
      split[i][size] = 0;
    }
  } while(d);
  
  *_nsplit = nsplit;
  *_split = split;
}

void split_templates(char *inp, char ***_templates, char ***_docs, size_t *_ntemplates, size_t *_ndocs) {
  char **split1;
  size_t nsplit1;
  delimiter_split(inp, DELIMITER1, &split1, &nsplit1);
  if(nsplit1 != 2) {
    printf("Error splitting input\n");
    return;
  }
  
  delimiter_split(split1[0], DELIMITER2, _templates, _ntemplates);
  delimiter_split(split1[1], DELIMITER2, _docs, _ndocs);

  for(size_t i = 0; i < nsplit1; i++) {
    free(split1[i]);
  }
  free(split1);  
}

xmlDocPtr parse_doc(char *inp, char *url, xmlDictPtr dict) {
  size_t size = strlen(inp);

  xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt(inp, size);
  
  if(dict) {
    xmlDictFree(ctxt->dict);
    ctxt->dict = dict;
    xmlDictReference(ctxt->dict);
  }
  
  xmlDocPtr doc =
      xmlCtxtReadMemory(ctxt, inp, size,
                        url, NULL,
                        XML_PARSE_NOENT | XML_PARSE_DTDATTR |
                            XML_PARSE_NOWARNING | XML_PARSE_NOCDATA);  
  xmlFreeParserCtxt(ctxt);
  
  return doc;
}

char **templates, **docs;
size_t ntemplates, ndocs;
size_t itemplates, idocs;

xmlDocPtr loader(const xmlChar *URI, xmlDictPtr dict, int options, void *ctxt, xsltLoadType type) {
  printf("In loader: %s\n", URI);
  
  xmlDocPtr ret = NULL;
  switch (type) {
    case XSLT_LOAD_DOCUMENT:
      // idocs = (idocs + 1) % ndocs;
      // ret = parse_doc(docs[idocs], (char *)URI, NULL);
      break;
    case XSLT_LOAD_STYLESHEET:
      itemplates = (itemplates + 1) % ntemplates;
      ret = parse_doc(templates[itemplates], (char *)URI, dict);
      break;
    default:
      break;
  }
  return ret;
}

uint64_t fuzz(char *name) {
  char *sample_bytes = NULL;
  uint32_t sample_size = 0;
  
  // read the sample either from file or
  // shared memory
  if(use_shared_memory) {
    sample_size = *(uint32_t *)(shm_data);
    if(sample_size > MAX_SAMPLE_SIZE) sample_size = MAX_SAMPLE_SIZE;
    sample_bytes = (char *)malloc(sample_size + 1);
    memcpy(sample_bytes, shm_data + sizeof(uint32_t), sample_size);
    sample_bytes[sample_size] = 0;
  } else {
    FILE *fp = fopen(name, "rb");
    if(!fp) {
      printf("Error opening %s\n", name);
      return 0;
    }
    fseek(fp, 0, SEEK_END);
    sample_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    sample_bytes = (char *)malloc(sample_size + 1);
    fread(sample_bytes, 1, sample_size, fp);
    sample_bytes[sample_size] = 0;
    fclose(fp);
  }
  
  templates = NULL;
  docs = NULL;
  ntemplates = 0; ndocs = 0;
  itemplates = 0; idocs = 0;
  
  xsltSetLoaderFunc(loader);
  
  split_templates(sample_bytes, &templates, &docs, &ntemplates, &ndocs);
  
  xmlDocPtr stylesheet_doc = parse_doc(templates[0], "main.xsl", NULL);
  xmlDocPtr source_doc = parse_doc(docs[0], "main.xml", NULL);
  
  int ret = 1;

  if(stylesheet_doc && source_doc) {
    xsltStylesheetPtr sheet = xsltParseStylesheetDoc(stylesheet_doc);
    printf("stylesheet: %p\n", sheet);
    if(sheet) {
        
      xsltTransformContextPtr transform_context =
        xsltNewTransformContext(sheet, source_doc);
      
      xsltSecurityPrefsPtr security_prefs = xsltNewSecurityPrefs();
      xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_FILE,
                                     xsltSecurityForbid);
      xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_CREATE_DIRECTORY,
                                  xsltSecurityForbid);
      xsltSetSecurityPrefs(security_prefs, XSLT_SECPREF_WRITE_NETWORK,
                                     xsltSecurityForbid);
      xsltSetCtxtSecurityPrefs(security_prefs, transform_context);
      
      transform_context->maxTemplateDepth = 300;
      
      xmlDocPtr result_doc = xsltApplyStylesheetUser(
        sheet, source_doc, NULL, NULL, NULL, transform_context);
        
        
      printf("result_doc: %p\n", result_doc);
      
      if(result_doc) {
        ret = 0;
      
        /*FILE *fp = fopen("out.txt", "w");
        xsltSaveResultToFile(fp, result_doc, sheet);
        fclose(fp);*/
        xmlFreeDoc(result_doc);
      }

      xsltFreeTransformContext(transform_context);
      xsltFreeSecurityPrefs(security_prefs);
    
      xsltFreeStylesheet(sheet);
    } else {
      xmlFreeDoc(source_doc);
      xmlFreeDoc(stylesheet_doc);
    }
  } else {
    printf("Parser error\n");
    
    if(source_doc) xmlFreeDoc(source_doc);
    if(stylesheet_doc) xmlFreeDoc(stylesheet_doc);
  }
  
  if(templates) {
    for(size_t i=0; i<ntemplates; i++) {
      free(templates[i]);
    }
    free(templates);
  }
  
  if(docs) {
    for(size_t i=0; i<ndocs; i++) {
      free(docs[i]);
    }
    free(docs);
  }

  if(sample_bytes) free(sample_bytes);
  
  return ret;
}

int main(int argc, char **argv)
{
  if(argc != 3) {
    printf("Usage: %s <-f|-m> <file or shared memory name>\n", argv[0]);
    return 0;
  }
  
  if(!strcmp(argv[1], "-m")) {
    use_shared_memory = true;
  } else if(!strcmp(argv[1], "-f")) {
    use_shared_memory = false;
  } else {
    printf("Usage: %s <-f|-m> <file or shared memory name>\n", argv[0]);
    return 0;
  }

  // map shared memory here as we don't want to do it
  // for every operation
  if(use_shared_memory) {
    if(!setup_shmem(argv[2])) {
      printf("Error mapping shared memory\n");
    }
  }
  
  xsltInit();

  JACKALOPE_FUZZ_LOOP(fuzz(argv[2]))
  
  return 0;
}

