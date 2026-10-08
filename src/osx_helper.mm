#include "osx_helper.h"
#include <QtGui>
#include <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>

QIcon osxGetIcon(const QString& extension)
{
    QIcon icon;
    @autoreleasepool
    {
        NSImage* image = [[NSWorkspace sharedWorkspace] iconForFileType:extension.toNSString()];
        NSData* tiff = [image TIFFRepresentation];
        if (tiff)
        {
            QPixmap pixmap;
            pixmap.loadFromData(QByteArray::fromNSData(tiff));
            icon = QIcon(pixmap);
        }
    }
    return icon;
}

void osxShowDockIcon()
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToForegroundApplication);
}

void osxHideDockIcon()
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    TransformProcessType(&psn, kProcessTransformToUIElementApplication);
}
