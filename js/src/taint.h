#ifndef taint_h
#define taint_h

#ifdef _TAINT_ON_

#include "jsapi.h"
typedef struct TaintNode
{

    TaintNode(JSContext *cx, const char* opname);
    ~TaintNode();


    void decrease();
    inline void increase() {
        refCount++;
    }

    void setPrev(TaintNode *other);
    void compileFrame(JSContext *cx);

    struct FrameStateElement;

    const char *op;
    uint32_t refCount;
    struct TaintNode *prev;
    JS::Heap<JS::Value> param1;
    JS::Heap<JS::Value> param2;
    FrameStateElement *stack;
private:
    TaintNode(const TaintNode& that);

} TaintNode;

typedef struct TaintStringRef
{
    uint32_t begin;
    uint32_t end;
    TaintNode *thisTaint;
    struct TaintStringRef *next;

    TaintStringRef() : begin(0), end(0), thisTaint(NULL), next(NULL) {};
    TaintStringRef(uint32_t s, uint32_t e, TaintNode* node = nullptr);
    TaintStringRef(const TaintStringRef &ref);
    ~TaintStringRef();

    inline void attachTo(TaintNode *node) {
        if(thisTaint) {
            thisTaint->decrease();
        }

        if(node) {
            node->increase();
        }
        
        thisTaint = node;
    }
} TaintStringRef;

//special allocators builder (using js_malloc)
TaintNode* taint_str_add_source_node(JSContext *cx, const char *fn);
TaintStringRef *taint_str_taintref_build();
TaintStringRef *taint_str_taintref_build(const TaintStringRef &ref);
TaintStringRef *taint_str_taintref_build(uint32_t begin, uint32_t end, TaintNode *node);

//wipe out the taint completely (this can free TaintNodes, too)
void taint_remove_all(TaintStringRef **start, TaintStringRef **end);

//set a (new) source
//str is assumed to not be tainted
template <typename TaintedT>
void taint_tag_source(TaintedT * str, const char* name, JSContext *cx = nullptr,
    uint32_t begin = 0, uint32_t end = 0)
{
    MOZ_ASSERT(!str->isTainted());

    if(str->Length() == 0) {
        return;
    }

    if(end == 0) {
        end = str->Length();
    }
    
    TaintNode *taint_node = taint_str_add_source_node(cx, name);
    TaintStringRef *newtsr = taint_str_taintref_build(begin, end, taint_node);
    str->addTaintRef(newtsr);
}

//partial taint copy
// - copy taint from source from frombegin until fromend
// - insert with offset
// - returns the start of the copied chain
// - optionally sets *taint_end to the end of the chain, if provided
// fromend = 0 -> copy all
TaintStringRef *taint_duplicate_range(TaintStringRef *src, TaintStringRef **taint_end = NULL,
    uint32_t frombegin = 0, int32_t offset = 0, uint32_t fromend = 0);

//create "space" at an offset
//e.g. push all taints after position behind by offset and split up crossing taints
//returns the last TaintStringRef /before/ the insertion point
TaintStringRef* taint_insert_offset(TaintStringRef *start, uint32_t position, uint32_t offset);

//copy & merge src in correct order into dst_{start,end}
//we assume no ranges are overlapping
void taint_copy_merge(TaintStringRef **dst_start, TaintStringRef **dst_end,
    TaintStringRef *src_start, uint32_t offset);

//remove a range of taint
TaintStringRef * taint_remove_range(TaintStringRef **start, TaintStringRef **end,
    uint32_t begin, uint32_t end_offset);

//exact taint copy when length of in/out do not match
// - needs to be called for every "token" in source
// - *target starts out with nullptr and continues to hold the
//   last taintref of the new chain
// - soff: offset of sidx to the start of the string (and with that,
//   taint reference indices)
// - return value has to be fed back into source, starts with
//   the top taintref of the source (and has to be ordered!)
TaintStringRef *
taint_copy_exact(TaintStringRef **target, 
    TaintStringRef *source, size_t sidx, size_t tidx, size_t soff = 0);

//---------------------------------
//these are helper functions for gecko code, because
//direct JSString calls are not yet available sometimes
//(JSString is only a forward declaration)

void taint_str_addref(JSString *str, TaintStringRef* ref);

//implemented for JSString, JSFlatString
template <typename T>
TaintStringRef *taint_get_top(T *str);

//----------------------------------

inline bool taint_istainted(TaintStringRef *const *start, TaintStringRef *const *end)
{
    MOZ_ASSERT(start && end);
    MOZ_ASSERT(!start == !end);
    return *start;
}

void taint_ff_end(TaintStringRef **end);

void taint_addtaintref(TaintStringRef *tsr, TaintStringRef **start, TaintStringRef **end);

//typical functions included to add taint functionality to string-like
//classes. additionally two TaintStringRef members are required.
#define TAINT_STRING_HOOKS(startTaint, endTaint)        \
    MOZ_ALWAYS_INLINE                                   \
    bool isTainted() const {                            \
        return taint_istainted(&startTaint, &endTaint); \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    TaintStringRef *getTopTaintRef() const {            \
        return startTaint;                              \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    TaintStringRef *getBottomTaintRef() const {         \
        return endTaint;                                \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    void addTaintRef(TaintStringRef *tsr)  {            \
        taint_addtaintref(tsr, &startTaint, &endTaint); \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    void removeRangeTaint(uint32_t start, uint32_t end) { \
        taint_remove_range(&startTaint, &endTaint,      \
            start, end);                                \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    void ffTaint() {                                    \
        taint_ff_end(&endTaint);                        \
    }                                                   \
                                                        \
    MOZ_ALWAYS_INLINE                                   \
    void removeAllTaint() {                             \
        if(isTainted())                                 \
            taint_remove_all(&startTaint, &endTaint);   \
    }

//--------------------------------------------


#endif


//--------------------------------------------

//in place operations and corresponding NOPs
#if _TAINT_ON_
//evaluate dst and copy taint to it.
#define TAINT_APPEND_TAINT(dst, src)                    \
[](decltype(dst) &taint_r, TaintStringRef* sourceref) -> decltype(dst)& {\
    if(sourceref /*&& taint_r.isTainted()*/) {          \
        taint_r.addTaintRef(taint_duplicate_range(sourceref));      \
    }                                                   \
    return taint_r;                                     \
}(dst, src)

//evaluate dst, clear, and copy taint
#define TAINT_ASSIGN_TAINT(dst, src)                    \
[](decltype(dst) &taint_r, TaintStringRef* sourceref) -> decltype(dst)& {\
    taint_r.removeAllTaint();                           \
    if(sourceref) {                                     \
        taint_r.addTaintRef(taint_duplicate_range(sourceref));      \
    }                                                   \
    return taint_r;                                     \
}(dst, src)

#else
#define TAINT_APPEND_TAINT(dst, src)  (dst)
#define TAINT_ASSIGN_TAINT(dst, src)  (dst)
#endif

//--------------------------------------------

#endif