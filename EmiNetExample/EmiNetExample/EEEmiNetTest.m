#import "EEEmiNetTest.h"

#import <EmiNet/EmiNet.h>

@interface EEEmiNetTest () <EmiSocketDelegate, EmiConnectionDelegate>

@end

@implementation EEEmiNetTest {
    NSMutableArray *_uncertainServerConnections;
    EmiConnection *_serverConnection;
    EmiConnection *_clientConnection;
}

- (void)runOnPort:(uint16_t)port
{
    EmiSocketConfig *sc = [[EmiSocketConfig alloc] init];
    sc.serverPort = port;
    sc.acceptConnections = YES;
    
    EmiSocket *socket = [[EmiSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_main_queue()];
    NSError *error;
    if (![socket startWithConfig:sc error:&error]) {
        NSLog(@"FAIL listen! %@", error);
        return;
    }
    
    if (![socket connectToHost:@"127.0.0.1"
                        onPort:port
                      delegate:self
                 delegateQueue:dispatch_get_main_queue()
                      userData:nil
                         error:&error]) {
        NSLog(@"FAIL connect! %@", error);
        return;
    }
    
    NSLog(@"Hej!");
    
    socket = nil;
}

- (void)emiSocket:(EmiSocket *)socket gotConnection:(EmiConnection *)connection
{
    NSLog(@"Server connection opened");
    if (_serverConnection)
        return;
    
    [_uncertainServerConnections addObject:connection];
    [connection setDelegate:self delegateQueue:dispatch_get_main_queue()];
}

- (void)emiConnectionOpened:(EmiConnection *)connection userData:(id)userData
{
    NSLog(@"Client connection opened");
    
    _clientConnection = connection;
    NSError *error;
    [_clientConnection send:[@"hello" dataUsingEncoding:NSUTF8StringEncoding]
                      error:&error];
}

- (void)emiConnectionFailedToConnect:(EmiSocket *)socket error:(NSError *)error userData:(id)userData
{
    NSLog(@"Connection failed");
}

- (void)emiConnectionDisconnect:(EmiConnection *)connection forReason:(EmiDisconnectReason)reason
{
    NSLog(@"Disconnect");
    
    [_uncertainServerConnections removeObject:connection];
    if (_serverConnection == connection) {
        _serverConnection = nil;
        return;
    }
}

- (void)emiConnectionMessage:(EmiConnection *)connection
            channelQualifier:(EmiChannelQualifier)channelQualifier
                        data:(NSData *)data
{
    if ([_uncertainServerConnections containsObject:connection] && !_serverConnection) {
        [_uncertainServerConnections removeAllObjects];
        _serverConnection = connection;
    }
    
    NSLog(@"Got message (fromClient %d)", connection == _clientConnection);
    NSError *error;
    [connection send:[@"hello" dataUsingEncoding:NSUTF8StringEncoding]
               error:&error];
}

@end
