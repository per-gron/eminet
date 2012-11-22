#define BUILDING_NODE_EXTENSION
#ifndef eminet_EmiObjectWrap
#define eminet_EmiObjectWrap

#include <node.h>

// The purpose of this class is to make node::ObjectWrap's
// Ref and Unref methods public, so that EmiBinding can access
// them.
class EmiObjectWrap : public node::ObjectWrap {
public:
    virtual void Ref() {
        node::ObjectWrap::Ref();
    }
    
    virtual void Unref() {
        node::ObjectWrap::Unref();
    }
};

#endif
