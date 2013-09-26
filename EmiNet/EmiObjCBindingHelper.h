//
//  EmiObjCBindingHelper.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-12.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiObjCBindingHelper_h
#define eminet_EmiObjCBindingHelper_h

#define DISPATCH_SYNC_OR_ASYNC(queue, block, method)        \
    {                                                       \
        dispatch_block_t DISPATCH_SYNC__block = (block);    \
        dispatch_queue_t DISPATCH_SYNC__queue = (queue);    \
        if (dispatch_get_current_queue() ==                 \
            DISPATCH_SYNC__queue) {                         \
            DISPATCH_SYNC__block();                         \
        }                                                   \
        else {                                              \
            dispatch_##method(DISPATCH_SYNC__queue,         \
                              DISPATCH_SYNC__block);        \
        }                                                   \
    }

#define DISPATCH_SYNC(queue, block)                         \
    DISPATCH_SYNC_OR_ASYNC(queue, block, sync)

#define DISPATCH_ASYNC(queue, block)                        \
    DISPATCH_SYNC_OR_ASYNC(queue, block, async)

#endif
