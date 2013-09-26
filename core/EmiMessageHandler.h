//
//  EmiMessageHandler.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-08.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiMessageHandler_h
#define eminet_EmiMessageHandler_h

#include "EmiMessageHeader.h"
#include "EmiUdpSocket.h"
#include "EmiNetUtil.h"

#include <netinet/in.h>

// The purpose of this class is to encapsulate behavior that is used
// by both EmiSock and EmiConn.
//
// I let Connection be a template argument as a hack to avoid a circular
// dependency between EmiConn.h and EmiMessageHandler.h
template<class Connection, class MessageHandlerDelegate, class Binding>
class EmiMessageHandler {
    typedef typename Binding::Error         Error;
    typedef typename Binding::TemporaryData TemporaryData;
    typedef EmiUdpSocket<Binding>           EUS;
    
private:
    // Private copy constructor and assignment operator
    inline EmiMessageHandler(const EmiMessageHandler& other);
    inline EmiMessageHandler& operator=(const EmiMessageHandler& other);
    
    MessageHandlerDelegate& _delegate;
    
    void sendSynRstAck(EUS *sock,
                       const sockaddr_storage& inboundAddress,
                       const sockaddr_storage& remoteAddress) {
        uint8_t buf[96];
        size_t size = EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG,
                                                              buf, sizeof(buf));
        ASSERT(0 != size); // size == 0 when the buffer was too small
        
        sock->sendData(inboundAddress, remoteAddress, buf, size);
    }
    
    bool processMessage(bool acceptConnections,
                        EmiTimeInterval now,
                        uint16_t inboundPort,
                        const sockaddr_storage& inboundAddress,
                        const sockaddr_storage& remoteAddress,
                        EUS *sock,
                        Connection*& conn,
                        bool unexpectedRemoteHost,
                        const char*& err,
                        const TemporaryData& data,
                        const uint8_t *rawData,
                        size_t offset,
                        const EmiMessageHeader& header,
                        size_t packetHeaderLength,
                        size_t dataOffset) {
        
        size_t actualRawDataOffset = dataOffset+packetHeaderLength;
        
#define ENSURE_CONN_TEST(test, msg)                         \
        do {                                                \
            if (!(test)) {                                  \
                /* We got a message that requires an open */\
                /* connection, but we don't have one.     */\
                /* This most likely means that the other  */\
                /* host honestly believes that the        */\
                /* connection is open. To spare the other */\
                /* host from having to wait for a full    */\
                /* connection timeout, which would have   */\
                /* happened if we silently ignored the    */\
                /* message, we respond with a SYN-RST-ACK */\
                /* message. The other host will interpret */\
                /* that as if this host closed the        */\
                /* connection, and allow it to re-connect */\
                /* immediately if desired.                */\
                /*                                        */\
                /* This is by the way probably not far    */\
                /* from the truth. A common way to        */\
                /* trigger this code is to restart the    */\
                /* server process.                        */\
                sendSynRstAck(sock,                         \
                              inboundAddress,               \
                              remoteAddress);               \
                                                            \
                err = "Got " msg " message but has no "     \
                      "open connection for that address";   \
                return false;                               \
            }                                               \
        } while (0)
#define ENSURE_CONN(msg)                                    \
        ENSURE_CONN_TEST(conn && !unexpectedRemoteHost, msg)
#define ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST(msg)       \
        ENSURE_CONN_TEST(conn, msg)
#define ENSURE(check, errStr)           \
        do {                            \
            if (!(check)) {             \
                err = errStr;           \
                return false;           \
            }                           \
        } while (0)
        
        bool prxFlag  = header.flags & EMI_PRX_FLAG;
        bool synFlag  = header.flags & EMI_SYN_FLAG;
        bool rstFlag  = header.flags & EMI_RST_FLAG;
        bool ackFlag  = header.flags & EMI_ACK_FLAG;
        bool sackFlag = header.flags & EMI_SACK_FLAG;
        
        if (!prxFlag && unexpectedRemoteHost) {
            ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("non-PRX");
            
            return conn->gotNonPrxMessageFromUnexpectedRemoteHost(remoteAddress);
        }
        
        if (prxFlag) {
            // This is some kind of proxy/P2P connection message
            
            if (!synFlag && !rstFlag && !ackFlag) {
                ENSURE_CONN("PRX");
                
                conn->gotPrx(now);
            }
            else if (synFlag && rstFlag && ackFlag) {
                ENSURE_CONN("PRX-RST-SYN-ACK");
                
                conn->gotPrxRstSynAck(now, rawData+actualRawDataOffset, header.length);
            }
            else if (!synFlag && rstFlag && ackFlag) {
                // We want to accept PRX-RST-ACK packets from hosts other than the
                // current remote host of the connection.
                ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("PRX-RST-ACK");
                
                conn->gotPrxRstAck(remoteAddress);
            }
            else if (synFlag && !rstFlag && !ackFlag) {
                // We want to accept PRX-SYN packets from hosts other than the
                // current remote host of the connection.
                ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("PRX-SYN");
                
                conn->gotPrxSyn(remoteAddress, rawData+actualRawDataOffset, header.length);
            }
            else if (synFlag && !rstFlag && ackFlag) {
                // We want to accept PRX-SYN-ACK packets from hosts other than the
                // current remote host of the connection.
                ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("PRX-SYN-ACK");
                
                conn->gotPrxSynAck(remoteAddress, rawData+actualRawDataOffset, header.length);
            }
            else {
                err = "Invalid message flags";
                return false;
            }
        }
        else if (synFlag && !rstFlag) {
            // This is an initiate connection message
            ASSERT(!unexpectedRemoteHost);
            ENSURE(0 == header.length,
                   "Got SYN message with message length != 0");
            ENSURE(!ackFlag, "Got SYN message with ACK flag");
            ENSURE(!sackFlag, "Got SYN message with SACK flag");
            
            if (conn && conn->isOpen() && conn->getOtherHostInitialSequenceNumber() != header.sequenceNumber) {
                // The connection is already open, and we get a SYN message with a
                // different initial sequence number. This probably means that the
                // other host has forgot about the connection we have open. Force
                // close it and ignore the packet.
                //
                // We can't invoke _delegate.makeServerConnection here, because we
                // might be executing on the EmiConn thread, and makeServerConnection
                // can only be called from the EmiSock thread.
                //
                // This is not a catastrophe; the other host will re-send the SYN
                // packet.
                conn->forceClose();
                conn = NULL;
            }
            else {
                if (!conn) {
                    // It is important to only do this check if we actually need
                    // to create a new connection object. Otherwise the check
                    // will prevent re-sending of SYN-RST messages in case the
                    // first packet was lost.
                    ENSURE(acceptConnections,
                           "Got SYN but this socket doesn't \
                           accept incoming connections");
                    
                    conn = _delegate.makeServerConnection(remoteAddress, inboundPort);
                }
                
                conn->opened(inboundAddress, now, header.sequenceNumber);
            }
        }
        else if (synFlag && rstFlag) {
            ASSERT(!unexpectedRemoteHost);
            if (ackFlag) {
                // This is a close connection ack message
                
                ENSURE(!sackFlag, "Got SYN-RST-ACK message with SACK flag");
                
                if (conn) {
                    conn->gotSynRstAck();
                    conn = NULL;
                }
                else {
                    // Receiving SYN-RST-ACK messages that don't have
                    // an open connection associated with them is part
                    // of the normal operation of the protocol.
                    //
                    // This can happen for instance when receiving
                    // duplicate SYN-RST-ACK messages.
                }
            }
            else {
                // This is a connection initiated message
                
                ENSURE(!sackFlag, "Got SYN-RST message with SACK flag");
                ENSURE_CONN("SYN-RST");
                ENSURE(conn->isOpening(), "Got SYN-RST message for open connection");
                
                if (!conn->gotSynRst(now, inboundAddress, header.sequenceNumber)) {
                    err = "Failed to process SYN-RST message";
                    return false;
                }
            }
        }
        else if (!synFlag && rstFlag) {
            // This is a close connection message
            
            ASSERT(!unexpectedRemoteHost);
            ENSURE(!ackFlag, "Got RST message with ACK flag");
            ENSURE(!sackFlag, "Got RST message with SACK flag");
            
            // Regardless of whether we still have a connection up,
            // respond with a SYN-RST-ACK message.
            sendSynRstAck(sock, inboundAddress, remoteAddress);
            
            // Note that this has to be done after we send the control
            // packet, since invoking gotRst might deallocate the sock
            // object.
            if (conn) {
                conn->gotRst();
                conn = NULL;
            }
        }
        else if (!synFlag && !rstFlag) {
            // This is a data message
            ASSERT(!unexpectedRemoteHost);
            ENSURE_CONN("data");
            
            conn->gotMessage(now, header, data, offset+actualRawDataOffset);
        }
        else {
            err = "Invalid message flags";
            return false;
        }
        
        return true;
    }
    
public:
    
    EmiMessageHandler(MessageHandlerDelegate& delegate) :
    _delegate(delegate) {}
    
    virtual ~EmiMessageHandler() {}
    
    void onMessage(bool acceptConnections,
                   EmiTimeInterval now,
                   EUS *sock,
                   bool unexpectedRemoteHost,
                   Connection *conn,
                   const sockaddr_storage& inboundAddress,
                   const sockaddr_storage& remoteAddress,
                   const TemporaryData& data,
                   size_t offset,
                   size_t len) {
        const char *err = NULL;
        
        uint16_t inboundPort(EmiNetUtil::addrPortH(inboundAddress));
        
        const uint8_t *rawData(Binding::extractData(data)+offset);
        
        EmiPacketHeader packetHeader;
        size_t packetHeaderLength;
        if (!EmiPacketHeader::parse(rawData, len, &packetHeader, &packetHeaderLength)) {
            err = "Invalid packet header";
            goto error;
        }
        
        if (conn && !unexpectedRemoteHost) {
            if (!conn->gotPacket(now, inboundAddress, packetHeader, len)) {
                // This happens when inboundAddress was invalid.
                return;
            }
        }
        
        if (packetHeaderLength == len) {
            // This is a heartbeat packet.
            //
            // We don't need to do anything here; all necessary processing
            // has already been done in the call to gotPacket.
        }
        else if (len < packetHeaderLength + EMI_MESSAGE_HEADER_MIN_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            size_t msgOffset = 0;
            size_t dataOffset;
            EmiMessageHeader header;
            while (msgOffset < len-packetHeaderLength) {
                if (!EmiMessageHeader::parseNextMessage(rawData+packetHeaderLength,
                                                        len-packetHeaderLength,
                                                        &msgOffset,
                                                        &dataOffset,
                                                        &header)) {
                    goto error;
                }
                
                if (!processMessage(acceptConnections,
                                    now,
                                    inboundPort, inboundAddress,
                                    remoteAddress,
                                    sock, conn, unexpectedRemoteHost,
                                    err,
                                    data, rawData,
                                    offset,
                                    header,
                                    packetHeaderLength,
                                    dataOffset)) {
                    goto error;
                }
            }
        }
        
        return;
    error:
        
        return;
    }
};

#endif
