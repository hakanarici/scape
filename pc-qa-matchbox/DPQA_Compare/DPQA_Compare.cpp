
#include <tclap/CmdLine.h>

#include <stdio.h>
#include <string>

#include "Comparison.h"

using namespace std;


int main(int argc, char* argv[])
{
	try
	{  
		// init comandline parser
			TCLAP::CmdLine cmd("Command description message", ' ', "0.9");

			TCLAP::UnlabeledValueArg<std::string> file1Arg("file1", "file 1 that should be compared", true, "", "file1", false);
			cmd.add( file1Arg );

			TCLAP::UnlabeledValueArg<std::string> file2Arg("file2", "file 2 that should be compared", true, "", "file2", false);
			cmd.add( file2Arg );

			TCLAP::ValueArg<int> argLevel("l","level","Comparison Level (1-4)",false,0,"int");
			cmd.add( argLevel );

			TCLAP::SwitchArg argVerbose("v","verbose","Provide additional debugging output",false);
			cmd.add( argVerbose );

		// init comparison
			Comparison* comp = new Comparison();

		// add tasks
			comp->addCommandLineArgs(&cmd);

		// parse arguments
			cmd.parse( argc, argv );

			// enable/disable verbose output
			VerboseOutput::verbose = argVerbose.getValue();
			
			string file1   = file1Arg.getValue();
			string file2   = file2Arg.getValue();

			comp->setLevel(argLevel.getValue());

			// compare xml files
			comp->read(&file1, &file2);
			comp->parseCommandLineArgs();
			
		// execute comparison
			comp->execute();

		// output results
			comp->writeOutput();

	} 
	catch (exception &e)  // catch any exceptions
	{
		cerr << "\n*** Comparison failed!\n";
		cerr << "    Reason: " << e.what() << "\n\n";
		exit(1);
	}
}