#import "EEAppDelegate.h"

#import "EEEmiNetTest.h"

@implementation EEAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    // Override point for customization after application launch.
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    EEEmiNetTest *test = [[EEEmiNetTest alloc] init];
    [test run];
    test = nil;
    
    return YES;
}

@end
