//
//  EmiMessageHandler.h
//  rock
//
//  Created by Per Eckerdal on 2012-06-08.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiMessageHandler_h
#define rock_EmiMessageHandler_h

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
                err = "Got "msg" message but has no "       \
                      "open connection for that address";   \
                return false;                               \
            }                                               \
        } while (0)
#define ENSURE_CONN(msg)                                    \
        ENSURE_CONN_TEST(conn && !unexpectedRemoteHost, msg)
#define ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST(msg)       \
        ENSURE_CONN_TEST(conn && !unexpectedRemoteHost, msg)
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
        
        if (prxFlag) {
            // This is some kind of proxy/P2P connection message
            
            if (!synFlag && !rstFlag && !ackFlag) {
                ENSURE_CONN("PRX");
                
                conn->gotPrx(now);
            }
            if (synFlag && rstFlag && ackFlag) {
                ENSURE_CONN("PRX-RST-SYN-ACK");
                
                conn->gotPrxRstSynAck(now, rawData+actualRawDataOffset, header.length);
            }
            if (!synFlag && rstFlag && ackFlag) {
                // We want to accept PRX-RST-ACK packets from hosts other than the
                // current remote host of the connection.
                ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("PRX-RST-ACK");
                
                conn->gotPrxRstAck(remoteAddress);
            }
            if (synFlag && !rstFlag && !ackFlag) {
                // We want to accept PRX-SYN packets from hosts other than the
                // current remote host of the connection.
                ENSURE_CONN_ALLOW_UNEXPECTED_REMOTE_HOST("PRX-SYN");
                
                conn->gotPrxSyn(remoteAddress, rawData+actualRawDataOffset, header.length);
            }
            if (synFlag && !rstFlag && ackFlag) {
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
            ENSURE(!unexpectedRemoteHost,
                   "Got SYN from unexpected remote host");
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
                ENSURE(acceptConnections,
                       "Got SYN but this socket doesn't \
                       accept incoming connections");
                
                if (!conn) {
                    conn = _delegate.makeServerConnection(remoteAddress, inboundPort);
                }
                
                conn->opened(inboundAddress, now, header.sequenceNumber);
            }
        }
        else if (synFlag && rstFlag) {
            if (ackFlag) {
                // This is a close connection ack message
                
                ENSURE(!sackFlag, "Got SYN-RST-ACK message with SACK flag");
                ENSURE(!unexpectedRemoteHost,
                       "Got SYN-RST-ACK from unexpected remote host");
                ENSURE(conn,
                       "Got SYN-RST-ACK message but has no open \
                       connection for that address. Ignoring the \
                       packet. (This is not really an error \
                       condition, it is part of normal operation \
                       of the protocol.)");
                
                conn->gotSynRstAck();
                conn = NULL;
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
            
            ENSURE(!ackFlag, "Got RST message with ACK flag");
            ENSURE(!sackFlag, "Got RST message with SACK flag");
            ENSURE(!unexpectedRemoteHost,
                   "Got RST from unexpected remote host");
            
            // Regardless of whether we still have a connection up,
            // respond with a SYN-RST-ACK message.
            uint8_t buf[96];
            size_t size = EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG,
                                                                  buf, sizeof(buf));
            ASSERT(0 != size); // size == 0 when the buffer was too small
            
            sock->sendData(inboundAddress, remoteAddress, buf, size);
            
            // Note that this has to be done after we send the control
            // packet, since invoking gotRst might deallocate the sock
            // object.
            //
            // TODO: Closing the socket here might be the wrong thing
            // to do. It might be better to let it stay alive for a full
            // connection timeout cycle, just to make sure that the other
            // host gets our SYN-RST-ACK response.
            if (conn) {
                conn->gotRst();
                conn = NULL;
            }
        }
        else if (!synFlag && !rstFlag) {
            // This is a data message
            ENSURE_CONN("data");
            
            conn->gotMessage(now, header, data, offset+actualRawDataOffset, /*dontFlush:*/false);
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
