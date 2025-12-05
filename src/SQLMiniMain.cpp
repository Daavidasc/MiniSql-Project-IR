#include <iostream>
#include <fstream>
#include "SQLMiniDriver.h"
#include "SQLMiniLexer.h"
#include "SQLMiniParser.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/TargetSelect.h"

using namespace antlr4;
using namespace llvm;
using namespace std;

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: ./MiniSQL <file.sql>" << endl;
        return -1;
    }

    ifstream ifile(argv[1]);
    if (!ifile.is_open()) return -1;

    ANTLRInputStream input(ifile);
    SQLMiniLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    SQLMiniParser parser(&tokens);

    tree::ParseTree *tree = parser.program();
    
    // Inicializar LLVM
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // Generar el IR Puro
    auto driver = new SQLMiniDriver();
    driver->visit(tree);

    // Opcional: Ver el IR generado (impresionante para debug)
    cout << "; --- GENERATED LLVM IR ---" << endl;
    driver->module->print(outs(), nullptr);
    cout << "; -------------------------" << endl;

    // Ejecutar JIT
    string errStr;
    ExecutionEngine *EE = EngineBuilder(driver->takeModule())
                            .setErrorStr(&errStr)
                            .create();

    if (!EE) {
        cerr << "Error JIT: " << errStr << endl;
        return 1;
    }

    // Ejecutar main() que contiene toda la lógica SQL compilada
    Function *mainFunc = driver->getModuleFunction();
    EE->runFunction(mainFunc, {});

    delete EE;
    delete driver;
    return 0;
}