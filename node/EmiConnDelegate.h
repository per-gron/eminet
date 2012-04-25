#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiConnDelegate_h
#define emilir_EmiConnDelegate_h

#include "../core/EmiTypes.h"
#include <uv.h>
#include <node.h>

class EmiConnection;

class EmiConnDelegate {
    EmiConnection& _conn;
    
public:
    EmiConnDelegate(EmiConnection& conn);
    
    void invalidate();
    
    void emiConnMessage(EmiChannelQualifier channelQualifier,
                        const v8::Local<v8::Object>& data,
                        size_t offset,
                        size_t size);
    
    void scheduleConnectionWarning(EmiTimeInterval warningTimeout);
    
    void emiConnLost();
    void emiConnRegained();
    void emiConnDisconnect(EmiDisconnectReason reason);
    
    inline EmiConnection& getConnection() { return _conn; }
    inline const EmiConnection& getConnection() const { return _conn; }
};

#endif
