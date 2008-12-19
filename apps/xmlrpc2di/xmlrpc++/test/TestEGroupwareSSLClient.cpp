#include "XmlRpc.h"
#include <iostream>

using namespace XmlRpc;
using namespace std;

int main ( int argc, char *argv[]) {
	XmlRpc::setVerbosity(3);

	if (argc != 5) {
		cout << "eGroupware SSL Client Test\n\n";
		cout << "usage: " << argv[0] << " host url user password\n";
		cout << "example: " << argv[0] << " www.egroupware.org \"/egroupware/xmlrpc.php\" demo guest\n";
		exit (0);
	}
	XmlRpcClient client (argv[1], 443, argv[2], true);

	XmlRpcValue result;
	XmlRpcValue params;
	params["username"] = argv[3];
	params["password"] = argv[4];
	if (!client.execute ("system.login", params, result)) {
		cout << "Failed.\n";
		return 0;
	} else {
		cout << result << "\n";
	}
	string sessionid = string(result["sessionid"]);
	string kp3 = string(result["kp3"]);
/*
	XmlRpcClient authClient (argv[1], 443, sessionid.c_str(), kp3.c_str(), argv[2], true);
	XmlRpcValue calParam;
	XmlRpcValue calResult;

	calParam["syear"] = "2001";
	calParam["smonth"] = "03";
	calParam["sday"] = "01";
	calParam["eyear"] = "2005";
	calParam["emonth"] = "04";
	calParam["eday"] = "25";

	if (!authClient.execute("calendar.bocalendar.search", calParam, calResult)) {
		cout << "No calendar events\n";
	} else {
		cout << calResult << "\n";
	}
*/

	XmlRpcValue logoutParam;
	XmlRpcValue logoutResult;
	XmlRpcClient client_logout (argv[1], 443, argv[2], true);

	logoutParam["sessionid"] = sessionid;
	logoutParam["kp3"] = kp3;

	if (!client_logout.execute("system.logout", logoutParam, logoutResult)) {
		cout << "failed to logout\n";
	} else {
		// params['GOODBYE'] == 'XOXO'
		cout << logoutResult << "\n";
	}

	return 1;
}

