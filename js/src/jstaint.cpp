/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "jstaint.h"

#include <codecvt>
#include <iostream>
#include <locale>
#include <string>
#include <utility>

#include "jsapi.h"
#include "js/Array.h"
#include "js/UniquePtr.h"
#include "vm/FrameIter.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/StringType.h"

using namespace JS;

const unsigned long max_length = 128;

static std::u16string ascii2utf16(const std::string& str) {
  std::u16string res;
  for (auto c : str)
    res.push_back(static_cast<char16_t>(c));
  return res;
}

std::u16string JS::taintarg(JSContext* cx, const char16_t* str)
{
  return std::u16string(str);
}

std::u16string JS::taintarg(JSContext* cx, HandleString str)
{
  if (!str) {
    return std::u16string();
  }
  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear)
    return std::u16string();

  js::UniquePtr<char16_t, JS::FreePolicy> buf(cx->pod_malloc<char16_t>(linear->length()));
  js::CopyChars(buf.get(), *linear);
  std::u16string result(buf.get(), std::min(linear->length(), max_length));
  return result;
}

std::u16string JS::taintarg_jsstring(JSContext* cx, JSString* const& str)
{
  if (!str) {
    return std::u16string();
  }
  JSLinearString* linear = str->ensureLinear(cx);
  if (!linear)
    return std::u16string();

  js::UniquePtr<char16_t, JS::FreePolicy> buf(cx->pod_malloc<char16_t>(linear->length()));
  js::CopyChars(buf.get(), *linear);
  std::u16string result(buf.get(), std::min(linear->length(), max_length));

  return result;
}

std::u16string JS::taintarg(JSContext* cx, HandleObject obj)
{
  RootedValue val(cx, ObjectValue(*obj));
  RootedString str(cx, ToString(cx, val));
  if (!str)
    return std::u16string();
  return taintarg(cx, str);
}

std::u16string JS::taintarg(JSContext* cx, HandleValue val)
{
  RootedString str(cx, ToString(cx, val));
  if (!str)
    return std::u16string();
  return taintarg(cx, str);
}

std::u16string JS::taintarg(JSContext* cx, int32_t num)
{
  RootedValue val(cx, Int32Value(num));
  return taintarg(cx, val);
}

std::vector<std::u16string> JS::taintargs(JSContext* cx, HandleValue val)
{
  std::vector<std::u16string> args;
  bool isArray;

  if (!IsArrayObject(cx, val, &isArray)) {
    return args;
  }

  if (isArray) {
    RootedObject array(cx, &val.toObject());
    uint32_t length;
    if (!GetArrayLength(cx, array, &length)) {
      return args;
    }
    for (uint32_t i = 0; i < length; ++i) {
      RootedValue v(cx);
      if (!JS_GetElement(cx, array, i, &v)) {
        continue;
      }
      args.push_back(taintarg(cx, v));
    }
  } else {
    args.push_back(taintarg(cx, val));
  }
  return args;
}

std::vector<std::u16string> JS::taintargs(JSContext* cx, HandleString str1, HandleString str2)
{
  std::vector<std::u16string> args;
  args.push_back(taintarg(cx, str1));
  args.push_back(taintarg(cx, str2));
  return args;
}

std::vector<std::u16string> JS::taintargs(JSContext* cx, HandleString arg)
{
  std::vector<std::u16string> args;
  args.push_back(taintarg(cx, arg));
  return args;
}

std::vector<std::u16string> JS::taintargs_jsstring(JSContext* cx, JSString* const& arg) 
{
  std::vector<std::u16string> args;
  args.push_back(taintarg_jsstring(cx, arg));
  return args;
}

std::vector<std::u16string> JS::taintargs_jsstring(JSContext* cx, JSString* const& str1, JSString* const& str2)
{
  std::vector<std::u16string> args;
  args.push_back(taintarg_jsstring(cx, str1));
  args.push_back(taintarg_jsstring(cx, str2));
  return args;
}

TaintLocation JS::TaintLocationFromContext(JSContext* cx)
{
  if (!cx) {
    return TaintLocation();
  }

  const char* filename = NULL;
  uint32_t line;
  uint32_t pos;
  RootedString function(cx);

  for (js::AllFramesIter i(cx); !i.done(); ++i) {
    if (i.hasScript()) {
      filename = JS_GetScriptFilename(i.script());
      line = PCToLineNumber(i.script(), i.pc(), &pos);
    } else {
      filename = i.filename();
      line = i.computeLine(&pos);
    }

    if (i.maybeFunctionDisplayAtom()) {
      function = i.maybeFunctionDisplayAtom();
    } else {
      function = cx->emptyString();
    }

    // Keep going down the stack if the function is self hosted
    if (strcmp(filename, "self-hosted") != 0) {
      break;
    }
  }

  if (filename == NULL) {
    return TaintLocation();
  }

  return TaintLocation(ascii2utf16(std::string(filename)), line, pos, taintarg(cx, function));
}

TaintOperation JS::TaintOperationFromContext(JSContext* cx, const char* name, JS::HandleValue args) {
  return TaintOperation(name, TaintLocationFromContext(cx), taintargs(cx, args));
}

TaintOperation JS::TaintOperationFromContext(JSContext* cx, const char* name, JS::HandleString arg ) {
  return TaintOperation(name, TaintLocationFromContext(cx), taintargs(cx, arg));
}

TaintOperation JS::TaintOperationFromContextJSString(JSContext* cx, const char* name, JSString* const& arg ) {
  return TaintOperation(name, TaintLocationFromContext(cx), taintargs_jsstring(cx, arg));
}

TaintOperation JS::TaintOperationFromContext(JSContext* cx, const char* name,
                                             JS::HandleString arg1, JS::HandleString arg2) {
  return TaintOperation(name, TaintLocationFromContext(cx), taintargs(cx, arg1, arg2));
}

TaintOperation JS::TaintOperationFromContext(JSContext* cx, const char* name,
                                             JSString* const& arg1, JSString* const & arg2) {
  return TaintOperation(name, TaintLocationFromContext(cx), taintargs_jsstring(cx, arg1, arg2));
}

TaintOperation JS::TaintOperationFromContext(JSContext* cx, const char* name) {
  return TaintOperation(name, TaintLocationFromContext(cx));
}

TaintOperation JS::TaintOperationFromContextNative(JSContext* cx, const char* name, JS::HandleValue args) {
  return TaintOperation(name, true, TaintLocationFromContext(cx), taintargs(cx, args));
}

TaintOperation JS::TaintOperationFromContextNative(JSContext* cx, const char* name, JS::HandleString arg) {
  return TaintOperation(name, true, TaintLocationFromContext(cx), taintargs(cx, arg));
}

TaintOperation JS::TaintOperationFromContextJSStringNative(JSContext* cx, const char* name, JSString* const& arg) {
  return TaintOperation(name, true, TaintLocationFromContext(cx), taintargs_jsstring(cx, arg));
}

TaintOperation JS::TaintOperationFromContextNative(JSContext* cx, const char* name,
                                             JS::HandleString arg1, JS::HandleString arg2) {
  return TaintOperation(name, true, TaintLocationFromContext(cx), taintargs(cx, arg1, arg2));
}

TaintOperation JS::TaintOperationFromContextNative(JSContext* cx, const char* name,
                                             JSString* const& arg1, JSString* const & arg2) {
  return TaintOperation(name, true, TaintLocationFromContext(cx), taintargs_jsstring(cx, arg1, arg2));
}

TaintOperation JS::TaintOperationFromContextNative(JSContext* cx, const char* name) {
  return TaintOperation(name, true, TaintLocationFromContext(cx));
}

void JS::MarkTaintedFunctionArguments(JSContext* cx, JSFunction* function, const CallArgs& args)
{
  if (!function)
    return;

  RootedValue name(cx);
  if (function->displayAtom()) {
    name = StringValue(function->displayAtom());
  }

  std::u16string sourceinfo(u"unknown");
  if (function->isInterpreted() && function->hasBaseScript()) {
    RootedScript script(cx, function->existingScript());
    if (script) {
      int lineno = script->lineno();
      js::ScriptSource* source = script->scriptSource();
      if (source && source->filename()) {
        std::string filename(source->filename());
        sourceinfo = ascii2utf16(filename) + u":" + ascii2utf16(std::to_string(lineno));
      }
    }
  }

  TaintLocation location = TaintLocationFromContext(cx);
  for (unsigned i = 0; i < args.length(); i++) {
    if (args[i].isString()) {
      RootedString arg(cx, args[i].toString());
      if (arg->isTainted()) {
        // Is there a more Mozilla way to do this?
        std::wstring_convert<std::codecvt_utf8<char16_t>,char16_t> convert;
        std::string fname_u8 = convert.to_bytes(taintarg(cx, name));
        arg->taint().extend(
          TaintOperation(fname_u8.c_str(), location,
                         { taintarg(cx, name), sourceinfo, taintarg(cx, i) } ));
      }
    }
  }
}

// Print a message to stdout.
void JS::TaintFoxReport(JSContext* cx, const char* msg)
{
  JS_ReportWarningUTF8(cx, "%s", msg);
}
