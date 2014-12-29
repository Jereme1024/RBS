#include <iostream>
#include <unistd.h>
#include <string>

int main()
{
	std::cout << "Content-Type: text/html\n\n";

	std::cout << R"(
	<html>
	<head>
	<meta http-equiv="Content-Type" content="text/html; charset=big5" />
	<title>Network Programming Homework 3</title>
	</head>
	<body bgcolor=#336699>
	<font face="Courier New" size=2 color=#FFFF99>
	<table width="800" border="1">
	<tr>
	<td>140.113.210.145</td><td>140.113.210.145</td><td>140.113.210.145</td></tr>
	<tr>
	<td valign="top" id="m0"></td><td valign="top" id="m1"></td><td valign="top" id="m2"></td></tr>
	</table>

	<script>document.all['m0'].innerHTML += "****************************************************************<br>";</script>
	)";

	std::cout.flush();

	sleep(2);

	std::cout << R"(<script>document.all['m1'].innerHTML += "****************************************************************<br>";</script>
	<script>document.all['m0'].innerHTML += "** Welcome to the information server, dist5.csie.nctu.edu.tw. **<br>";</script>
	<script>document.all['m0'].innerHTML += "** You are in the directory, /.<br>";</script>
	<script>document.all['m1'].innerHTML += "** You are in the directory, /.<br>";</script>
	<script>document.all['m2'].innerHTML += "****************************************************************<br>";</script>
	)";

	std::cout.flush();

	sleep(2);

	std::cout << R"(
	<script>document.all['m1'].innerHTML += "% <b>removetag test.html</b><br>";</script>
	<script>document.all['m1'].innerHTML += "<br>";</script>
	<script>document.all['m1'].innerHTML += "Test<br>";</script>
	<script>document.all['m1'].innerHTML += "This is a test program<br>";</script>
	<script>document.all['m1'].innerHTML += "for ras.<br>";</script>
	<script>document.all['m1'].innerHTML += "<br>";</script>
	<script>document.all['m1'].innerHTML += "<br>";</script>
	<script>document.all['m1'].innerHTML += "% <b></b><br>";</script>
	)";
	
	std::cout << R"(
	</font>
	</body>
	</html>
	)";

	return 0;
}
