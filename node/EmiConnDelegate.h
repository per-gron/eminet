#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiConnDelegate_h
#define emilir_EmiConnDelegate_h

#include "../core/EmiTypes.h"
#include <uv.h>
#include <node.h>

class EmiConnection;

class EmiConnDelegate {
    EmiConnection& _conn;
    
    // We must use pointers to malloc'ed memory for the timers,
    // because the memory can't be freed in this class' destructor,
    // it has to be freed in the close callback of the timer handle,
    // which is called asynchronously.
    uv_timer_t *_tickTimer;
    uv_timer_t *_heartbeatTimer;
    uv_timer_t *_rtoTimer;
    uv_timer_t *_connectionTimer;
    uv_timer_t *_rateLimitTimer;
    
    EmiTimeInterval _warningTimeoutWhenWarningTimerWasScheduled;
    EmiTimeInterval _rtoWhenRtoTimerWasScheduled;
    
    static void rateLimitTimeout(uv_timer_t *handle, int status);
    static void warningTimeout(uv_timer_t *handle, int status);
    static void connectionTimeout(uv_timer_t *handle, int status);
    static void tickTimeout(uv_timer_t *handle, int status);
    static void heartbeatTimeout(uv_timer_t *handle, int status);
    static void rtoTimeout(uv_timer_t *handle, int status);
    
public:
    EmiConnDelegate(EmiConnection& conn);
    
    void invalidate();
    
    void emiConnMessage(EmiChannelQualifier channelQualifier,
                        const v8::Local<v8::Object>& data,
                        size_t offset,
                        size_t size);
    
    void startRateLimitTimer(EmiTimeInterval rate);
    
    void scheduleConnectionWarning(EmiTimeInterval warningTimeout);
    void scheduleConnectionTimeout(EmiTimeInterval interval);
    
    void ensureTickTimeout(EmiTimeInterval interval);
    
    void scheduleHeartbeatTimeout(EmiTimeInterval interval);
    
    void ensureRtoTimeout(EmiTimeInterval rto);
    void invalidateRtoTimeout();
    
    void emiConnLost();
    void emiConnRegained();
    void emiConnDisconnect(EmiDisconnectReason reason);
    
    inline EmiConnection& getConnection() { return _conn; }
    inline const EmiConnection& getConnection() const { return _conn; }
};

#endif
