#include "string.h"
#include <iostream>
#include <fstream>

#include "grammar.h"

#include "../../prng.h"
#include "../../sample.h"

using namespace std;

Grammar::BinaryRW::BinaryRW() {
  bytes = (unsigned char*)malloc(BINARY_RW_INITIAL_SIZE);
  size_allocated = BINARY_RW_INITIAL_SIZE;
  size_current = 0;
  read_pos = 0;
}

Grammar::BinaryRW::BinaryRW(size_t size, unsigned char* data) {
  bytes = (unsigned char*)malloc(size);
  memcpy(bytes, data, size);
  size_allocated = size;
  size_current = size;
  read_pos = 0;
}

Grammar::BinaryRW::~BinaryRW() {
  if (bytes) free(bytes);
}

void Grammar::BinaryRW::WriteData(unsigned char* data, size_t size) {
  while (size_current + size > size_allocated) {
    size_allocated *= 2;
    bytes = (unsigned char*)realloc(bytes, size_allocated);
  }
  memcpy(bytes + size_current, data, size);
  size_current += size;
}

void Grammar::BinaryRW::WriteSize(size_t size) {
  uint64_t size64 = size;
  WriteData((unsigned char *)&size64, sizeof(size64));
}

void Grammar::BinaryRW::WriteString(std::string* s) {
  unsigned char* data = (unsigned char *)s->data();
  size_t size = s->size();
  WriteSize(size);
  WriteData(data, size);
}

int Grammar::BinaryRW::ReadData(unsigned char* data, size_t size) {
  if (read_pos + size > size_current) {
    return 0;
  }
  memcpy(data, bytes + read_pos, size);
  read_pos += size;
  return 1;
}

int Grammar::BinaryRW::ReadSize(size_t *size) {
  uint64_t size64;
  if (!ReadData((unsigned char*)&size64, sizeof(size64))) return 0;
  *size = (size_t)size64;
  return 1;
}

int Grammar::BinaryRW::ReadString(std::string* s) {
  size_t size;
  if (!ReadSize(&size)) return 0;
  if (read_pos + size > size_current) {
    return 0;
  }
  s->assign((char *)(bytes + read_pos), size);
  read_pos += size;
  return 1;
}

void Grammar::TreeNode::Clear() {
  type = STRINGTYPE;
  string = NULL;
  for (auto iter = children.begin(); iter != children.end(); iter++) {
    delete *iter;
  }
  children.clear();
}

Grammar::TreeNode::~TreeNode() {
  Clear();
}

Grammar::TreeNode::TreeNode(const TreeNode &other) {
  this->type = other.type;
  if (this->type == STRINGTYPE) {
    this->string = other.string;
  } else {
    this->symbol = other.symbol;
  }

  for (auto iter = other.children.begin(); iter != other.children.end(); iter++) {
    TreeNode* child = new TreeNode(**iter);
    this->children.push_back(child);
  }
}

Grammar::TreeNode& Grammar::TreeNode::operator=(const TreeNode& other) {
  Clear();

  this->type = other.type;
  if (this->type == STRINGTYPE) {
    this->string = other.string;
  } else {
    this->symbol = other.symbol;
  }

  for (auto iter = other.children.begin(); iter != other.children.end(); iter++) {
    TreeNode* child = new TreeNode(**iter);
    this->children.push_back(child);
  }

  return *this;
}

void Grammar::TreeNode::Replace(TreeNode* other) {
  Clear();

  this->type = other->type;
  if (this->type == STRINGTYPE) {
    this->string = other->string;
  } else {
    this->symbol = other->symbol;
  }

  for (auto iter = other->children.begin(); iter != other->children.end(); iter++) {
    this->children.push_back(*iter);
  }
  other->children.clear();
  delete other;
}

size_t Grammar::TreeNode::NumNodes() {
  size_t ret = 1;

  for (auto iter = children.begin(); iter != children.end(); iter++) {
    Grammar::TreeNode* child = *iter;
    ret += child->NumNodes();
  }

  return ret;
}

Grammar::Grammar() {
  constants["lt"] = "<";
  constants["gt"] = ">";
  constants["hash"] = "#";
  constants["cr"] = "\x0d";
  constants["lf"] = "\x0a";
  constants["crlf"] = "\x0d\x0a";
  constants["space"] = " ";
  constants["tab"] = "\t";
}

int Grammar::HexStringToString(std::string &hex, std::string &out) {
  if (hex.size() % 2) return 0;

  unsigned char cur_value;
  unsigned char prev_value;

  for (size_t i = 2; i < hex.size(); i++) {
    unsigned char c = hex[i];
    if ('0' <= c && c <= '9')
      cur_value = c - '0';
    else if ('a' <= c && c <= 'f')
      cur_value = c - 'a' + 10;
    else if ('A' <= c && c <= 'F')
      cur_value = c - 'A' + 10;
    else return 0;

    if (i % 2) {
      c = (prev_value << 4) + cur_value;
      out.push_back(c);
    } else {
      prev_value = cur_value;
    }
  }
  return 1;
}

Grammar::Symbol *Grammar::GetOrCreateSymbol(std::string &name) {
  auto iter = symbols.find(name);
  if (iter == symbols.end()) {
    Symbol *symbol = new Symbol(name);
    symbols[name] = symbol;
    if (name.find("repeat_") == 0) {
      string repeat_name = name.substr(7, name.length() - 7);
      symbol->repeat = 1;
      symbol->repeat_symbol = GetOrCreateSymbol(repeat_name);
      symbol->repeat_symbol->used = true;
    }
    return symbol;
  } else {
    return iter->second;
  }
}

Grammar::Symbol *Grammar::GetSymbol(std::string &name) {
  auto iter = symbols.find(name);
  if (iter == symbols.end()) {
    return NULL;
  } else {
    return iter->second;
  }
}

int Grammar::CheckGrammar() {
  int ret = 1;
  for (auto iter = symbols.begin(); iter != symbols.end(); iter++) {
    Symbol *symbol = iter->second;
    if (symbol->generators.empty() && (!symbol->repeat)) {
      printf("Error: no generators for symbol %s\n", symbol->name.c_str());
      ret = 0;
    }
    if (!symbol->used && (symbol->name != "root")) {
      printf("Warning: unused grammar symbol: %s\n", symbol->name.c_str());
    }
  }
  return ret;
}

void Grammar::AnalyzeGrammar() {
  int ret = 1;
  for (auto iter = symbols.begin(); iter != symbols.end(); iter++) {
    Symbol* symbol = iter->second;
    symbol->can_be_empty = 0;
    for (auto iter2 = symbol->generators.begin(); iter2 != symbol->generators.end(); iter2++) {
      Rule* rule = &(*iter2);
      if (rule->parts.empty()) {
        symbol->can_be_empty = 1;
        break;
      }
    }
  }
}

int Grammar::AddRulePart(Grammar::Rule *rule, Grammar::NodeType type, std::string &value) {
  RulePart newpart;
  if (type == SYMBOLTYPE) {
    // see if we can convert it to a string
    auto iter = constants.find(value);
    if (iter != constants.end()) {
      return AddRulePart(rule, STRINGTYPE, iter->second);
    } else if (value.find("0x") == 0) {
      string decoded;
      if (!HexStringToString(value, decoded)) return 0;
      return AddRulePart(rule, STRINGTYPE, decoded);
    } else {
      // ok, actually a symbol
      newpart.type = SYMBOLTYPE;
      newpart.value = value;
      newpart.symbol = GetOrCreateSymbol(value);
      newpart.symbol->used = true;
      rule->parts.push_back(newpart);
    }
  } else {
    // check if we can merge this with the previous string
    if (rule->parts.size()) {
      RulePart * lastpart = &rule->parts[rule->parts.size() - 1];
      if (lastpart->type == STRINGTYPE) {
        lastpart->value.append(value);
        return 1;
      }
    }
    newpart.type = STRINGTYPE;
    newpart.value = value;
    newpart.symbol = NULL;
    rule->parts.push_back(newpart);
  }
  return 1;
}

int Grammar::Read(const char *filename) {
  string line;
  ifstream file(filename);
  if (!file) {
    printf("Error reading %s\n", filename);
    return 0;
  }
  int ret = 1;
  int lineno = 0;
  while (getline(file, line)) {
    if (!ParseGrammarLine(line, lineno)) {
      ret = 0;
      break;
    }
    lineno++;
  }
  file.close();

  AnalyzeGrammar();
  if (ret) ret = CheckGrammar();
  return ret;
}

int Grammar::ParseGrammarLine(string &line, int lineno) {
  // remove comments and trim
  string cleanline = line.substr(0, line.find('#', 0));
  size_t last_character = cleanline.find_last_not_of(" \t");
  if (last_character == string::npos) {
    // empy line
    return 1;
  }
  cleanline = cleanline.substr(0, last_character + 1);

  Rule newrule;

  const char *str = cleanline.c_str();
  ParseState state = LINESTART;
  const char *symbolstart = NULL;
  string symbolname;
  int ret = 1;
  while (1) {
    switch (state) {

    case LINESTART:
    {
      switch (*str) {
      case 0x20:
      case 0x09:
        break;
      case '<':
        state = GENERATORSYMBOL;
        symbolstart = str + 1;
        break;
      default:
        ret = 0;
        break;
      }
    }
    break;

    case GENERATORSYMBOL:
    {
      switch (*str) {
      case '>':
        state = GENERATORSYMBOLEND;
        symbolname.assign(symbolstart, str - symbolstart);
        if (symbolname.empty()) ret = 0;
        else newrule.generates = symbolname;
        break;
      case 0:
        ret = 0;
        break;
      default:
        break;
      }
    }
    break;

    case GENERATORSYMBOLEND:
    {
      switch (*str) {
      case 0x20:
      case 0x09:
        break;
      case '=':
        state = EQUAL;
        break;
      default:
        ret = 0;
        break;
      }
    }
    break;

    case EQUAL:
    {
      switch (*str) {
      case 0x20:
      case 0x09:
        state = EQUALSPACE;
        break;
      case '<':
        state = SYMBOL;
        symbolstart = str + 1;
        break;
      case 0:
        // empty rule
        break;
      default:
        state = STRING;
        symbolstart = str;
        break;
      }
    }
    break;

    case EQUALSPACE:
    {
      switch (*str) {
      case '<':
        state = SYMBOL;
        symbolstart = str + 1;
        break;
      case 0:
        // empty rule
        break;
      default:
        state = STRING;
        symbolstart = str;
        break;
      }
    }
    break;

    case SYMBOL:
    {
      switch (*str) {
      case '>':
        state = SYMBOLEND;
        symbolname.assign(symbolstart, str - symbolstart);
        if (symbolname.empty()) ret = 0;
        if (!AddRulePart(&newrule, SYMBOLTYPE, symbolname)) ret = 0;
        break;
      case 0:
        ret = 0;
        break;
      default:
        break;
      }
    }
    break;

    case SYMBOLEND:
    {
      switch (*str) {
      case '<':
        state = SYMBOL;
        symbolstart = str + 1;
        break;
      case 0:
        break;
      default:
        state = STRING;
        symbolstart = str;
        break;
      }
    }
    break;

    case STRING:
    {
      switch (*str) {
      case '<':
        symbolname.assign(symbolstart, str - symbolstart);
        if (!AddRulePart(&newrule, STRINGTYPE, symbolname)) ret = 0;
        state = SYMBOL;
        symbolstart = str + 1;
        break;
      case 0:
        symbolname.assign(symbolstart, str - symbolstart);
        if (!AddRulePart(&newrule, STRINGTYPE, symbolname)) ret = 0;
        break;
      default:
        break;
      }
    }
    break;

    default:
      break;
    }

    if (ret == 0) break;
    if (!*str) break;
    str++;
  }

  if (ret) {
    Symbol *symbol = GetOrCreateSymbol(newrule.generates);
    symbol->generators.push_back(newrule);
  } else {
    printf("Error parsing grammar on line %d: %s", lineno, line.c_str());
  }

  return ret;
}

Grammar::TreeNode *Grammar::GenerateStringNode(std::string *string) {
  TreeNode *node = new TreeNode();
  node->type = STRINGTYPE;
  node->string = string;
  return node;
}

Grammar::TreeNode *Grammar::GenerateTree(Symbol *symbol, PRNG *prng, int depth) {
  if (depth > MAX_DEPTH) {
    // printf("Warning: Maximum recursion depth reached while generating symbol %s\n", symbol->name.c_str());
    return NULL;
  }

  //printf("Generating %s\n", symbol->name.c_str());

  TreeNode* node = new TreeNode();
  node->type = SYMBOLTYPE;
  node->symbol = symbol;

  if (symbol->repeat) {
    while (1) {
      if (prng->RandReal() > REPEAT_PROBABILITY) break;
      TreeNode* child = GenerateTree(symbol->repeat_symbol, prng, depth + 1);
      if (!child) {
        node->Clear();
        delete node;
        return NULL;
      }
      node->children.push_back(child);
    }
    return node;
  }

  size_t num_generators = symbol->generators.size();
  Rule &generator = symbol->generators[prng->Rand() % num_generators];

  for (auto iter = generator.parts.begin(); iter != generator.parts.end(); iter++) {
    if (iter->type == SYMBOLTYPE) {
      TreeNode* child = GenerateTree(iter->symbol, prng, depth + 1);
      if (!child) {
        node->Clear();
        delete node;
        return NULL;
      }
      node->children.push_back(child);
    } else {
      node->children.push_back(GenerateStringNode(&iter->value));
    }
  }
  return node;
}

Grammar::TreeNode *Grammar::GenerateTree(const char *symbol, PRNG *prng) {
  string symbol_name(symbol);
  Symbol *s = GetSymbol(symbol_name);
  if (!s) {
    printf("Error: unknown symbol %s\n", symbol);
    return NULL;
  }
  return GenerateTree(s, prng);
}

int Grammar::GenerateString(const char* symbol, PRNG* prng, std::string *out) {
  Grammar::TreeNode* tree = GenerateTree(symbol, prng);
  if (!tree) return 0;
  ToString(tree, *out);
  delete tree;
  return 1;
}


void Grammar::ToString(TreeNode *tree, std::string &out) {
  if (tree->type == STRINGTYPE) {
    out.append(*tree->string);
  } else {
    for (auto iter = tree->children.begin(); iter != tree->children.end(); iter++) {
      ToString(*iter, out);
    }
  }
}

void Grammar::EncodeTree(TreeNode* tree, BinaryRW* rw) {
  unsigned char type = (char)tree->type;
  rw->WriteData(&type, 1);
  if (tree->type == STRINGTYPE) {
    rw->WriteString(tree->string);
  } else {
    rw->WriteString(&tree->symbol->name);
  }
  rw->WriteSize(tree->children.size());
  for (auto iter = tree->children.begin(); iter != tree->children.end(); iter++) {
    EncodeTree(*iter, rw);
  }
}

Grammar::TreeNode* Grammar::DecodeTree(BinaryRW* rw) {
  unsigned char type;
  if(!rw->ReadData(&type, 1)) return NULL;

  TreeNode* tree = new TreeNode();
  tree->type = (NodeType)type;

  if (tree->type == STRINGTYPE) {
    std::string str;
    if (!rw->ReadString(&str)) {
      delete tree;
      return NULL;
    }
    tree->string = GetStringFromCache(str);
  } else {
    string symbolname;
    if (!rw->ReadString(&symbolname)) {
      delete tree;
      return NULL;
    }
    tree->symbol = GetSymbol(symbolname);
    if (!tree->symbol) {
      delete tree;
      return NULL;
    }
  }

  size_t nchildren;
  if (!rw->ReadSize(&nchildren)) {
    delete tree;
    return NULL;
  }

  for (size_t i = 0; i < nchildren; i++) {
    TreeNode* child = DecodeTree(rw);
    if (!child) {
      delete tree;
      return NULL;
    }
    tree->children.push_back(child);
  }

  return tree;
}

std::string* Grammar::GetStringFromCache(std::string& s) {
  auto iter = string_cache.find(s);
  if (iter == string_cache.end()) {
    string* sp = new string(s);
    string_cache[s] = sp;
    return sp;
  }
  return iter->second;
}

void Grammar::EncodeSample(TreeNode* tree, Sample* sample) {
  std::string sample_string;
  ToString(tree, sample_string);
  BinaryRW rw;
  rw.WriteString(&sample_string);
  EncodeTree(tree, &rw);
  sample->Init((char*)rw.GetData(), rw.GetSize());
}

Grammar::TreeNode* Grammar::DecodeSample(Sample* sample) {
  BinaryRW rw(sample->size, (unsigned char*)sample->bytes);
  std::string sample_string;
  if (!rw.ReadString(&sample_string)) {
    return NULL;
  }
  Grammar::TreeNode* tree = DecodeTree(&rw);
  if (!tree) {
    return NULL;
  }
  return tree;
}
