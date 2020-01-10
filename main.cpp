#include "stdafx.h"
#include "ProxyVirus.h"

int main(int argc, char* argv[])
{
	ProxyVirus virus;

	if (virus.StartProxyServer() == false)
		return -1;

	getchar();
	return 0;
}
