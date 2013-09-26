#import "EEAppDelegate.h"

#import "EEEmiNetTest.h"

@implementation EEAppDelegate {
    EEEmiNetTest *_test;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    // Override point for customization after application launch.
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    _test = [[EEEmiNetTest alloc] init];
    [_test runOnPort:1025];
    
    return YES;
}

@end
