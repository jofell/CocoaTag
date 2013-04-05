//
//  CocoaTagAppDelegate.m
//  CocoaTag
//
//  Created by Johnno Loggie on 20/11/2010.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import "CocoaTagAppDelegate.h"

extern "C"
{
	#import "MifareFormat.h"
	#import "MifareURL.h"
}

#include <ndefmessage.h>

#include <QtCore/QLatin1String>

@implementation CocoaTagAppDelegate

@synthesize window;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	// Insert code here to initialize your application 
}

- (IBAction) formatTag: (id) sender
{
	format_tag();
	[self LogString: @"\n"];
}

#define RECORD_HEADER_1_RECORD 0xd1
const uint8_t ndef_msg[15] = {
    RECORD_HEADER_1_RECORD, 0x01, 0x0b, 0x55, 0x01, 'j', 'o', 'h',
    'n', 'n', 'o', '.', 'c', 'o', 'm'
};


- (IBAction) writeURL: (id) sender
{
	NDEFMessage msg;
	msg.appendRecord(NDEFRecord::createUriRecord(    QString::fromUtf8( txtURL.stringValue.UTF8String)  ));
	msg.appendRecord(NDEFRecord::createTextRecord( QString(QLatin1String("Hello, world!")), QString(QLatin1String("en-US")) ));
	QByteArray output = msg.toByteArray();
	write_ndef( (const uint8_t*) output.data(),output.size());
	[self LogString: @"Wrote message\n"];
}


- (void) LogString: (NSString*) myText;
{
    NSRange endRange;
    endRange.location = txtLog.textStorage.length;
    endRange.length = 0;
    [txtLog replaceCharactersInRange:endRange withString:myText];
    endRange.length = myText.length;
    [txtLog scrollRangeToVisible:endRange];
}

- (IBAction) writeVCard:(id)sender
{
    NDEFMessage msg;
    
    NSOpenPanel *dialog = [NSOpenPanel openPanel];
    dialog.canChooseFiles = YES;
    dialog.canCreateDirectories = NO;
    dialog.allowsMultipleSelection = NO;
    
    NSURL *selURL = nil;
    
    if ( [dialog runModal] == NSOKButton )
    {
        selURL = dialog.URL;
    }
    
    if (selURL == nil) return;
    
    // Get the string from the file
    NSString *fileContents = [NSString stringWithContentsOfURL:selURL
                                                      encoding:NSUTF8StringEncoding
                                                         error:nil];
    
    QByteArray byteContents(fileContents.UTF8String);
    msg.appendRecord(NDEFRecord::createMimeRecord(QString("text/x-vCard"), byteContents));
    msg.appendRecord(NDEFRecord::createMimeRecord(QString("text/vcard"), byteContents));
    msg.appendRecord(NDEFRecord::createMimeRecord(QString("text/directory;profile=vCard"), byteContents));
	QByteArray output = msg.toByteArray();
	write_ndef( (const uint8_t*) output.data(),output.size());
	[self LogString: @"Wrote message\n"];
}

-(void)writeNDEF:(id)sender {
    NDEFMessage msg;
    
    NSOpenPanel *dialog = [NSOpenPanel openPanel];
    dialog.canChooseFiles = YES;
    dialog.canCreateDirectories = NO;
    dialog.allowsMultipleSelection = NO;
    
    NSURL *selURL = nil;
    
    if ( [dialog runModal] == NSOKButton )
    {
        selURL = dialog.URL;
    }
    
    if (selURL == nil) return;
    
    // Get the string from the file
    NSString *fileContents = [NSString stringWithContentsOfURL:selURL
                                                      encoding:NSUTF8StringEncoding
                                                         error:nil];
    
    QByteArray output(fileContents.UTF8String);
    write_ndef( (const uint8_t*) output.data(),output.size());
	[self LogString: @"Wrote message\n"];
}

@end
