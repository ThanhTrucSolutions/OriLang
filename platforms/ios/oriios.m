// Generic iOS UIKit host for Ori UI apps.
// Build script copies app.orb into the .app bundle as a resource.
#define ORI_AS_LIB
#include "../../core/orivm.c"
#import <UIKit/UIKit.h>

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

@interface OriAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow* window;
@property(nonatomic,strong) UIStackView* stack;
@property(nonatomic,strong) UITextField* edit;
@property(nonatomic,strong) NSMutableArray<NSDictionary*>* actions;
@end

@implementation OriAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application; (void)launchOptions;
    self.actions = [NSMutableArray array];
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

    UIViewController* vc = [UIViewController new];
    vc.view.backgroundColor = UIColor.systemBackgroundColor;
    UIScrollView* scroll = [UIScrollView new];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [vc.view addSubview:scroll];
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.topAnchor],
        [scroll.bottomAnchor constraintEqualToAnchor:vc.view.safeAreaLayoutGuide.bottomAnchor],
    ]];

    self.stack = [UIStackView new];
    self.stack.axis = UILayoutConstraintAxisVertical;
    self.stack.alignment = UIStackViewAlignmentFill;
    self.stack.spacing = 10;
    self.stack.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll addSubview:self.stack];
    [NSLayoutConstraint activateConstraints:@[
        [self.stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:16],
        [self.stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-16],
        [self.stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:16],
        [self.stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-16],
        [self.stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-32],
    ]];

    NSString* image = [[NSBundle mainBundle] pathForResource:@"app" ofType:@"orb"];
    if(image.length > 0 && ori_boot(image.UTF8String, 0, NULL) == 0){
        char* ui = ori_call_str("render", "");
        [self build:ui];
        free(ui);
    } else {
        [self build:"text|Unable to load app.orb\n"];
    }

    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)clear {
    for(UIView* v in self.stack.arrangedSubviews.copy){
        [self.stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [self.actions removeAllObjects];
    self.edit = nil;
}

- (UILabel*)label:(NSString*)text {
    UILabel* l = [UILabel new];
    l.text = text;
    l.numberOfLines = 0;
    l.font = [UIFont systemFontOfSize:17 weight:UIFontWeightRegular];
    return l;
}

- (UIButton*)button:(NSString*)title actionIndex:(NSInteger)idx {
    UIButton* b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    b.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    b.tag = idx;
    [b addTarget:self action:@selector(fire:) forControlEvents:UIControlEventTouchUpInside];
    b.backgroundColor = UIColor.secondarySystemBackgroundColor;
    b.layer.cornerRadius = 8;
    b.contentEdgeInsets = UIEdgeInsetsMake(10, 12, 10, 12);
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
                self.edit = [UITextField new];
                self.edit.placeholder = ns(ph);
                self.edit.borderStyle = UITextBorderStyleRoundedRect;
                [self.stack addArrangedSubview:self.edit];
            } else if(strcmp(type, "btn") == 0){
                char ev[64], arg[512], cap[512];
                q = field(q, ev, sizeof ev); q = field(q, arg, sizeof arg); field(q, cap, sizeof cap);
                NSInteger idx = self.actions.count;
                [self addActionEvent:ns(ev) arg:ns(arg)];
                [self.stack addArrangedSubview:[self button:ns(cap) actionIndex:idx]];
            } else if(strcmp(type, "item") == 0){
                char tap[64], del[64], arg[512], cap[512];
                q = field(q, tap, sizeof tap); q = field(q, del, sizeof del); q = field(q, arg, sizeof arg); field(q, cap, sizeof cap);
                UIStackView* row = [UIStackView new];
                row.axis = UILayoutConstraintAxisHorizontal;
                row.spacing = 8;
                row.alignment = UIStackViewAlignmentFill;
                NSInteger tapIdx = self.actions.count;
                [self addActionEvent:ns(tap) arg:ns(arg)];
                UIButton* left = [self button:ns(cap) actionIndex:tapIdx];
                NSInteger delIdx = self.actions.count;
                [self addActionEvent:ns(del) arg:ns(arg)];
                UIButton* right = [self button:@"X" actionIndex:delIdx];
                [right.widthAnchor constraintEqualToConstant:52].active = YES;
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
    NSString* arg = [action[@"edit"] boolValue] ? (self.edit.text ?: @"") : action[@"arg"];
    char* ui = ori_call2([action[@"ev"] UTF8String], [arg UTF8String]);
    [self build:ui];
    free(ui);
}

@end

int main(int argc, char* argv[]){
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([OriAppDelegate class]));
    }
}
