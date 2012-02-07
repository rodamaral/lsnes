#ifdef __WXMAC__
#include <ApplicationServices/ApplicationServices.h>
#endif

void bring_app_foreground()
{
#ifdef __WXMAC__
	//On Mac OS X, make the application foreground application.
	ProcessSerialNumber PSN;
	GetCurrentProcess(&PSN);
	TransformProcessType(&PSN,kProcessTransformToForegroundApplication);
#endif
}
