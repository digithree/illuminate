//
//  fileDialog.mm
//  Illuminate
//
//  Created by Simon Kenny on 28/09/2020.
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#include <string>
#include <vector>
#include "fileDialog.h"

std::string openFileDialog(char const * const defaultPathAndFile) {
    NSOpenPanel* dialog = [NSOpenPanel openPanel];
    [dialog setLevel:CGShieldingWindowLevel()];
    
    // Set array of file types
    /*
    std::vector<std::string> filters = std::vector<std::string>();
    filters.push_back("sav");
    NSMutableArray * fileTypesArray = [NSMutableArray array];
    for (int i = 0; i < filters.size(); i++)
    {
        NSString * filt =[NSString stringWithUTF8String:filters[i].c_str()];
        [fileTypesArray addObject:filt];
    }
     */
    
    // Enable options in the dialog.
    [dialog setCanChooseFiles:YES];
    //[dialog setAllowedFileTypes:fileTypesArray];
    if (defaultPathAndFile)
    {
        [dialog setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:defaultPathAndFile]]];
    }
    
    if ([dialog runModal] == NSOKButton) {
        return std::string([[[dialog URL] path] UTF8String]);
    }
    return "";
}

std::string saveFileDialog(char const * const defaultPathAndFile) {
    NSSavePanel* dialog = [NSSavePanel savePanel];
    [dialog setLevel:CGShieldingWindowLevel()];
    
    if (defaultPathAndFile)
    {
        [dialog setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:defaultPathAndFile]]];
    }
    
    if ([dialog runModal] == NSOKButton) {
        return std::string([[[dialog URL] path] UTF8String]);
    }
    return "";
}
