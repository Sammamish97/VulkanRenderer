#include "Demo.h"
#include <iostream>
int main()
{
	Demo demo;

	try
	{
		demo.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}