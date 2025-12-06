#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "SQLMiniDriver.h"
#include "SQLMiniLexer.h"
#include "SQLMiniParser.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/FileSystem.h" 
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/AliasAnalysis.h"

using namespace antlr4;
using namespace llvm;
using namespace std;

// Función para guardar archivo
void saveModuleToFile(Module* module, const string& filename) {
    std::error_code EC;
    raw_fd_ostream dest(filename, EC, sys::fs::OF_None);
    if (EC) {
        cerr << "Error al escribir archivo " << filename << ": " << EC.message() << endl;
        return;
    }
    module->print(dest, nullptr);
    cout << "-> [Archivo Generado]: " << filename << endl;
}

// Pipeline de optimización O2
void OptimizeModule(Module* module) {
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    MPM.run(*module, MAM);
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        cerr << "Uso: ./MiniSQL <archivo.sql> [--raw]" << endl;
        return -1;
    }

    string sqlFile = "";
    bool saveRaw = false;

    // Parsear argumentos
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--raw") {
            saveRaw = true;
        } else {
            sqlFile = arg;
        }
    }

    if (sqlFile.empty()) {
        cerr << "Error: Falta el archivo SQL." << endl;
        return -1;
    }

    ifstream ifile(sqlFile);
    if (!ifile.is_open()) return -1;

    ANTLRInputStream input(ifile);
    SQLMiniLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    SQLMiniParser parser(&tokens);

    tree::ParseTree *tree = parser.program();
    
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    auto driver = new SQLMiniDriver();
    
    // 1. Generar IR (Crudo)
    driver->visit(tree);

    // --- OPCIÓN --raw: Guardar versión cruda en archivo ---
    if (saveRaw) {
        cout << "[INFO] Flag '--raw' activa. Guardando IR sin optimizar..." << endl;
        saveModuleToFile(driver->module.get(), "ir_crudo.ll");
    }

    // 2. Optimizar (Siempre)
    OptimizeModule(driver->module.get());

    // 3. Imprimir IR Optimizado en Consola (Por defecto)
    cout << "\n; --- OPTIMIZED LLVM IR ---" << endl;
    driver->module->print(outs(), nullptr);
    cout << "; -------------------------\n" << endl;

    // 4. Ejecutar JIT
    string errStr;
    ExecutionEngine *EE = EngineBuilder(driver->takeModule())
                            .setErrorStr(&errStr)
                            .setEngineKind(EngineKind::JIT)
                            .create();

    if (!EE) {
        cerr << "Error JIT: " << errStr << endl;
        return 1;
    }
    
    Function *mainFunc = driver->getModuleFunction(); 
    EE->runFunction(mainFunc, {});

    delete EE;
    delete driver;
    return 0;
}