// The Open and Save dialogs of macOS (AppKit's NSOpenPanel and NSSavePanel,
// Apple's public documentation), for FileBrowser::run_native. Own design;
// see PROVENANCE.md.

#include "file_dialog_macos.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace tisma::ui {

bool macos_file_dialog(bool save, const std::string& title, const std::vector<std::string>& extensions,
                       const std::filesystem::path& folder, const std::string& name, std::filesystem::path& chosen) {
    @autoreleasepool {
        NSSavePanel* panel = save ? [NSSavePanel savePanel] : [NSOpenPanel openPanel];
        if (!save) {
            NSOpenPanel* open = (NSOpenPanel*)panel;
            open.canChooseFiles = YES;
            open.canChooseDirectories = NO;
            open.allowsMultipleSelection = NO;
        }
        panel.title = [NSString stringWithUTF8String:title.c_str()];
        if (!folder.empty())
            panel.directoryURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:folder.string().c_str()]
                                            isDirectory:YES];
        // The extensions as the allowed types (empty: every file); in Save
        // mode the first one is added when the name has none, and the panel
        // asks before replacing a file.
        NSMutableArray<UTType*>* types = [NSMutableArray array];
        for (const std::string& e : extensions) {
            const std::string bare = !e.empty() && e.front() == '.' ? e.substr(1) : e;
            if (UTType* t = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:bare.c_str()]])
                [types addObject:t];
        }
        if (types.count > 0)
            panel.allowedContentTypes = types;
        if (save) {
            panel.canCreateDirectories = YES;
            panel.nameFieldStringValue = [NSString stringWithUTF8String:name.c_str()];
        }
        NSWindow* owner = [NSApp keyWindow];
        const NSModalResponse answer = [panel runModal];
        if (owner)
            [owner makeKeyAndOrderFront:nil];
        if (answer != NSModalResponseOK || panel.URL == nil) {
            chosen.clear();
            return true;
        }
        chosen = std::filesystem::path(std::string(panel.URL.fileSystemRepresentation));
        return true;
    }
}

} // namespace tisma::ui
