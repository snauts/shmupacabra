#include "bundle_path.h"

void
change_dir_to_bundle(void)
{
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        {
                NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
                [[NSFileManager defaultManager] changeCurrentDirectoryPath:resourcePath];
        }
        [pool drain];
}
