// Generic macOS AppKit host for Ori UI apps.
// Build script copies app.orb into the .app bundle as a resource.
#define ORI_AS_LIB
#include "../../core/orivm.c"
#import <Cocoa/Cocoa.h>

static NSString* ns(const char* s){
    return [NSString stringWithUTF8String:s ? s : ""] ?: @"";
}

static const char* field(const char* p, char* out, int cap){
    int i = 0;
    while(*p && *p != '|'){
        if(i < cap - 1) out[i++] = *p;
        p++;
    }
    out[i] = 0;
    if(*p == '|') p++;
    return p;
}

@interface OriAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic,strong) NSWindow* window;
@property(nonatomic,strong) NSStackView* stack;
@property(nonatomic,strong) NSTextField* edit;
@property(nonatomic,strong) NSMutableArray<NSDictionary*>* actions;
@end

@implementation OriAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    self.actions = [NSMutableArray array];
    NSRect frame = NSMakeRect(0, 0, 492, 560);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
    self.window.title = @"Ori";
    [self.window center];

    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:frame];
    scroll.hasVerticalScroller = YES;
    self.stack = [[NSStackView alloc] initWithFrame:NSMakeRect(16, 16, 440, 520)];
    self.stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.stack.alignment = NSLayoutAttributeLeading;
    self.stack.spacing = 8;
    self.stack.edgeInsets = NSEdgeInsetsMake(16, 16, 16, 16);
    scroll.documentView = self.stack;
    self.window.contentView = scroll;

    NSString* image = [[NSBundle mainBundle] pathForResource:@"app" ofType:@"orb"];
    if(image.length > 0 && ori_boot(image.UTF8String, 0, NULL) == 0){
        char* ui = ori_call_str("render", "");
        [self build:ui];
        free(ui);
    } else {
        [self build:"text|Unable to load app.orb\n"];
    }

    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

- (void)clear {
    for(NSView* v in self.stack.arrangedSubviews.copy){
        [self.stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [self.actions removeAllObjects];
    self.edit = nil;
}

- (NSTextField*)label:(NSString*)text {
    NSTextField* l = [NSTextField labelWithString:text];
    l.lineBreakMode = NSLineBreakByWordWrapping;
    l.maximumNumberOfLines = 0;
    [l.widthAnchor constraintEqualToConstant:420].active = YES;
    return l;
}

- (NSButton*)button:(NSString*)title actionIndex:(NSInteger)idx {
    NSButton* b = [NSButton buttonWithTitle:title target:self action:@selector(fire:)];
    b.tag = idx;
    b.bezelStyle = NSBezelStyleRounded;
    return b;
}

- (void)addActionEvent:(NSString*)ev arg:(NSString*)arg {
    [self.actions addObject:@{
        @"ev": ev ?: @"",
        @"arg": arg ?: @"",
        @"edit": @([arg isEqualToString:@"@edit"])
    }];
}

- (void)build:(const char*)spec {
    [self clear];
    const char* p = spec ? spec : "";
    while(*p){
        const char* nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if(len > 0){
            char line[1024];
            if(len > 1000) len = 1000;
            memcpy(line, p, len);
            line[len] = 0;
            char type[16];
            const char* q = field(line, type, sizeof type);
            if(strcmp(type, "text") == 0){
                [self.stack addArrangedSubview:[self label:ns(q)]];
            } else if(strcmp(type, "edit") == 0){
                char ph[256]; field(q, ph, sizeof ph);
                self.edit = [NSTextField textFieldWithString:@""];
                self.edit.placeholderString = ns(ph);
                [self.edit.widthAnchor constraintEqualToConstant:420].active = YES;
                [self.stack addArrangedSubview:self.edit];
            } else if(strcmp(type, "btn") == 0){
                char ev[64], arg[512], cap[512];
                q = field(q, ev, sizeof ev); q = field(q, arg, sizeof arg); field(q, cap, sizeof cap);
                NSInteger idx = self.actions.count;
                [self addActionEvent:ns(ev) arg:ns(arg)];
                NSButton* b = [self button:ns(cap) actionIndex:idx];
                [b.widthAnchor constraintEqualToConstant:420].active = YES;
                [self.stack addArrangedSubview:b];
            } else if(strcmp(type, "item") == 0){
                char tap[64], del[64], arg[512], cap[512];
                q = field(q, tap, sizeof tap); q = field(q, del, sizeof del); q = field(q, arg, sizeof arg); field(q, cap, sizeof cap);
                NSStackView* row = [[NSStackView alloc] init];
                row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
                row.spacing = 8;
                NSInteger tapIdx = self.actions.count;
                [self addActionEvent:ns(tap) arg:ns(arg)];
                NSButton* left = [self button:ns(cap) actionIndex:tapIdx];
                [left.widthAnchor constraintEqualToConstant:340].active = YES;
                NSInteger delIdx = self.actions.count;
                [self addActionEvent:ns(del) arg:ns(arg)];
                NSButton* right = [self button:@"X" actionIndex:delIdx];
                [right.widthAnchor constraintEqualToConstant:72].active = YES;
                [row addArrangedSubview:left];
                [row addArrangedSubview:right];
                [self.stack addArrangedSubview:row];
            }
        }
        if(!nl) break;
        p = nl + 1;
    }
}

- (void)fire:(id)sender {
    NSInteger idx = [sender tag];
    if(idx < 0 || idx >= (NSInteger)self.actions.count) return;
    NSDictionary* action = self.actions[(NSUInteger)idx];
    NSString* arg = [action[@"edit"] boolValue] ? (self.edit.stringValue ?: @"") : action[@"arg"];
    char* ui = ori_call2([action[@"ev"] UTF8String], [arg UTF8String]);
    [self build:ui];
    free(ui);
}

@end

int main(int argc, const char** argv){
    (void)argc; (void)argv;
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        OriAppDelegate* delegate = [OriAppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
