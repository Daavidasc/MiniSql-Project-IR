#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "SQLMiniBaseVisitor.h" 

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream> // Necesario para guardar el esquema
#include <sstream> // Necesario para leer el esquema

using namespace antlr4;
using namespace llvm;
using namespace std;

// --- Estructuras para Metadata ---
enum DataType { INT_T, DECIMAL_T, VARCHAR_T, BOOLEAN_T };

struct ColumnInfo {
    std::string name;
    DataType type;
};

struct TableInfo {
    StructType* llvmStructType; 
    std::vector<ColumnInfo> columns;
};

class SQLMiniDriver : public SQLMiniBaseVisitor {
private:
    std::map<std::string, TableInfo> symbolTable; 

    // Helpers de Tipos
    DataType getDataTypeFromText(const std::string& typeText) {
        if (typeText.find("int") != std::string::npos) return INT_T;
        if (typeText.find("decimal") != std::string::npos) return DECIMAL_T;
        if (typeText.find("varchar") != std::string::npos) return VARCHAR_T;
        if (typeText.find("boolean") != std::string::npos) return BOOLEAN_T;
        throw std::runtime_error("Unknown type: " + typeText);
    }

    Type* getLLVMType(DataType dt) {
        switch (dt) {
            case INT_T: return Type::getInt32Ty(context);
            case DECIMAL_T: return Type::getDoubleTy(context);
            case BOOLEAN_T: return Type::getInt1Ty(context);
            case VARCHAR_T: return PointerType::get(context, 0); 
        }
        return Type::getVoidTy(context);
    }

    std::string getPrintFormat(DataType dt) {
        switch (dt) {
            case INT_T: return "%d";
            case DECIMAL_T: return "%.2f";
            case VARCHAR_T: return "%s";
            case BOOLEAN_T: return "%d";
        }
        return "";
    }

    std::string getScanFormat(DataType dt) {
        switch (dt) {
            case INT_T: return "%d";
            case DECIMAL_T: return "%lf";
            case VARCHAR_T: return "%s"; // %s lee hasta espacio. Usar %[^\t] si soportas espacios.
            case BOOLEAN_T: return "%d";
        }
        return "";
    }

    std::string getSeparatorLine(int colCount) {
        std::string line = "+";
        for (int i = 0; i < colCount; ++i) {
            line += std::string(22, '-') + "+"; // 22 guiones por columna
        }
        line += "\n";
        return line;
    }

    // --- NUEVO: Guardar esquema en disco ---
    void saveSchema(const std::string& tableName, const std::vector<ColumnInfo>& cols) {
        std::ofstream schemaFile("schema.txt", std::ios::app); // Append
        if (schemaFile.is_open()) {
            schemaFile << tableName;
            for (const auto& col : cols) {
                schemaFile << " " << col.name << ":" << (int)col.type;
            }
            schemaFile << "\n";
            schemaFile.close();
        }
    }

    // --- NUEVO: Cargar esquema al iniciar ---
    void loadSchema() {
        std::ifstream schemaFile("schema.txt");
        std::string line;
        while (std::getline(schemaFile, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string tableName, colToken;
            ss >> tableName;

            if (symbolTable.count(tableName)) continue; // Ya cargada

            std::vector<ColumnInfo> cols;
            std::vector<Type*> structFields;
            
            while (ss >> colToken) {
                size_t sep = colToken.find(':');
                std::string name = colToken.substr(0, sep);
                int typeInt = std::stoi(colToken.substr(sep + 1));
                DataType dt = (DataType)typeInt;
                
                cols.push_back({name, dt});
                structFields.push_back(getLLVMType(dt));
            }

            StructType* tableStruct = StructType::create(context, "struct." + tableName);
            tableStruct->setBody(structFields);
            symbolTable[tableName] = {tableStruct, cols};
            // std::cout << "Loaded schema for: " << tableName << std::endl;
        }
    }

public:
    LLVMContext context;
    std::unique_ptr<Module> module;
    std::unique_ptr<IRBuilder<>> irBuilder;
    
    FunctionCallee printfFunc, fopenFunc, fcloseFunc, fprintfFunc, fscanfFunc;
    Function* mainFunc;

    SQLMiniDriver()
      : module(std::make_unique<Module>("MiniSQL_IR", context)),
        irBuilder(std::make_unique<IRBuilder<>>(context)) {
            // CARGA AUTOMÁTICA AL INICIAR
            loadSchema();
        }

    Function* getModuleFunction() { return mainFunc; }
    std::unique_ptr<Module> takeModule() { return std::move(module); }

    virtual std::any visitProgram(SQLMiniParser::ProgramContext *ctx) {
        printfFunc = module->getOrInsertFunction("printf", FunctionType::get(Type::getInt32Ty(context), PointerType::get(context, 0), true));
        fopenFunc = module->getOrInsertFunction("fopen", FunctionType::get(PointerType::get(context, 0), {PointerType::get(context, 0), PointerType::get(context, 0)}, false));
        fcloseFunc = module->getOrInsertFunction("fclose", FunctionType::get(Type::getInt32Ty(context), {PointerType::get(context, 0)}, false));
        fprintfFunc = module->getOrInsertFunction("fprintf", FunctionType::get(Type::getInt32Ty(context), {PointerType::get(context, 0), PointerType::get(context, 0)}, true));
        fscanfFunc = module->getOrInsertFunction("fscanf", FunctionType::get(Type::getInt32Ty(context), {PointerType::get(context, 0), PointerType::get(context, 0)}, true));

        mainFunc = Function::Create(FunctionType::get(Type::getInt32Ty(context), false), Function::ExternalLinkage, "main", *module);
        BasicBlock *entry = BasicBlock::Create(context, "entry", mainFunc);
        irBuilder->SetInsertPoint(entry);

        visitChildren(ctx);

        irBuilder->CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
        return std::any();
    }

    virtual std::any visitCreate(SQLMiniParser::CreateContext *ctx) {
        std::string tableName = ctx->ID()->getText();
        
        // Evitar duplicados si ya existe
        if (symbolTable.count(tableName)) {
            Value* msg = irBuilder->CreateGlobalStringPtr("Table " + tableName + " already exists.\n");
            irBuilder->CreateCall(printfFunc, {msg});
            return std::any();
        }

        std::vector<Type*> structFields;
        std::vector<ColumnInfo> cols;

        StructType* tableStruct = StructType::create(context, "struct." + tableName);
        
        // Nota: Asumimos gramática corregida: ID tipoDato, O usas tipoDato ID según tu preferencia.
        // Aquí uso tu lógica original (tipo nombre o nombre tipo según hayas definido)
        // Adaptado a la gramática fija: columnDefinition: ID tipoDato
        for (auto const& colDef : ctx->columnDefinition()) {
             // Si usas gramática "ID tipoDato":
             std::string name = colDef->ID()->getText();
             std::string typeText = colDef->tipoDato()->getText();
             
             // Si usas gramática "tipoDato ID", invierte las líneas de arriba.
             
            DataType dt = getDataTypeFromText(typeText);
            structFields.push_back(getLLVMType(dt));
            cols.push_back({name, dt});
        }
        tableStruct->setBody(structFields);

        symbolTable[tableName] = {tableStruct, cols};
        
        // GUARDAR EN DISCO EL ESQUEMA
        saveSchema(tableName, cols);

        Value* msg = irBuilder->CreateGlobalStringPtr("Table '" + tableName + "' created.\n");
        irBuilder->CreateCall(printfFunc, {msg});

        return std::any();
    }

    virtual std::any visitInsert(SQLMiniParser::InsertContext *ctx) {
        std::string tableName = ctx->ID()->getText();
        if (symbolTable.find(tableName) == symbolTable.end()) {
            std::cerr << "Error: Table " << tableName << " not found (Load schema failed?)." << std::endl;
            return std::any();
        }
        TableInfo& info = symbolTable[tableName];

        Value* filename = irBuilder->CreateGlobalStringPtr(tableName + ".txt");
        Value* mode = irBuilder->CreateGlobalStringPtr("a"); 
        Value* filePtr = irBuilder->CreateCall(fopenFunc, {filename, mode});

        std::string fileFormatStr = "";
        std::vector<Value*> args;
        args.push_back(filePtr); 

        auto values = ctx->dato();
        for (size_t i = 0; i < values.size(); ++i) {
            std::string valText = values[i]->getText();
            DataType dt = info.columns[i].type;
            
            if (i > 0) fileFormatStr += "\t";
            fileFormatStr += getPrintFormat(dt);

            Value* valToStore = nullptr;
            if (dt == INT_T) valToStore = ConstantInt::get(Type::getInt32Ty(context), std::stoi(valText));
            else if (dt == DECIMAL_T) valToStore = ConstantFP::get(Type::getDoubleTy(context), std::stod(valText));
            else if (dt == BOOLEAN_T) valToStore = ConstantInt::get(Type::getInt1Ty(context), valText == "true");
            else if (dt == VARCHAR_T) {
                std::string content = valText.substr(1, valText.size()-2); 
                valToStore = irBuilder->CreateGlobalStringPtr(content);
            }
            args.push_back(valToStore);
        }
        fileFormatStr += "\n"; 
        args.insert(args.begin() + 1, irBuilder->CreateGlobalStringPtr(fileFormatStr));
        irBuilder->CreateCall(fprintfFunc, args);
        irBuilder->CreateCall(fcloseFunc, {filePtr});
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("Row saved to " + tableName + ".txt\n")});

        return std::any();
    }

    virtual std::any visitSelect(SQLMiniParser::SelectContext *ctx) {
        std::string tableName = ctx->ID(ctx->ID().size() - 1)->getText();
        
        if (symbolTable.find(tableName) == symbolTable.end()) {
            Value* errMsg = irBuilder->CreateGlobalStringPtr("Error: Table " + tableName + " not found in schema.\n");
            irBuilder->CreateCall(printfFunc, {errMsg});
            return std::any();
        }
        
        TableInfo& info = symbolTable[tableName];

        // 1. Filtrar columnas seleccionadas
        std::vector<std::pair<int, ColumnInfo>> selectedCols;
        if (ctx->ESTRELLA()) {
            for (size_t i = 0; i < info.columns.size(); ++i) selectedCols.push_back({i, info.columns[i]});
        } else {
             for (size_t i = 0; i < ctx->ID().size() - 1; ++i) {
                std::string colName = ctx->ID(i)->getText();
                for(size_t j=0; j<info.columns.size(); ++j) {
                    if(info.columns[j].name == colName) selectedCols.push_back({j, info.columns[j]});
                }
             }
        }

        // --- GENERAR CABECERA TIPO TABLA ---
        std::string separator = getSeparatorLine(selectedCols.size());
        
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("\nQuery result for: " + tableName + "\n")});
        
        // Linea superior (+----------------+)
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});

        // Nombres de columnas con padding fijo
        std::string headerStr = "|";
        for (const auto& item : selectedCols) {
            // Truco: formateamos el string en C++ para que tenga espacios de relleno
            std::string name = item.second.name;
            if (name.length() > 20) name = name.substr(0, 20); // Cortar si es muy largo
            else name.append(20 - name.length(), ' '); // Rellenar con espacios
            
            headerStr += " " + name + " |"; 
        }
        headerStr += "\n";
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(headerStr)});

        // Linea media (+----------------+)
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});

        // --- PREPARAR LECTURA DE ARCHIVO ---
        Value* filename = irBuilder->CreateGlobalStringPtr(tableName + ".txt");
        Value* mode = irBuilder->CreateGlobalStringPtr("r");
        Value* filePtr = irBuilder->CreateCall(fopenFunc, {filename, mode});

        BasicBlock* fileOkBB = BasicBlock::Create(context, "file_ok", mainFunc);
        BasicBlock* fileErrBB = BasicBlock::Create(context, "file_err", mainFunc);
        BasicBlock* afterSelectBB = BasicBlock::Create(context, "after_select", mainFunc);

        Value* isNull = irBuilder->CreateICmpEQ(filePtr, Constant::getNullValue(filePtr->getType()));
        irBuilder->CreateCondBr(isNull, fileErrBB, fileOkBB);

        // Caso Error: Archivo no existe
        irBuilder->SetInsertPoint(fileErrBB);
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("| No data found (Empty table)                  |\n")});
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});
        irBuilder->CreateBr(afterSelectBB);

        // Caso OK: Leer datos
        irBuilder->SetInsertPoint(fileOkBB);
        std::vector<Value*> readBuffers;
        std::string scanFmt = "";

        // Preparar buffers para fscanf (igual que antes)
        for (size_t i = 0; i < info.columns.size(); ++i) {
            DataType dt = info.columns[i].type;
            if (i > 0) scanFmt += "\t"; 
            if (dt == VARCHAR_T) {
                scanFmt += "%s"; 
                Value* arrAlloc = irBuilder->CreateAlloca(ArrayType::get(Type::getInt8Ty(context), 256));
                readBuffers.push_back(irBuilder->CreateBitCast(arrAlloc, PointerType::get(context, 0)));
            } else {
                scanFmt += getScanFormat(dt);
                readBuffers.push_back(irBuilder->CreateAlloca(getLLVMType(dt)));
            }
        }
        
        BasicBlock* loopCond = BasicBlock::Create(context, "loop_cond", mainFunc);
        BasicBlock* loopBody = BasicBlock::Create(context, "loop_body", mainFunc);
        BasicBlock* loopEnd = BasicBlock::Create(context, "loop_end", mainFunc);

        irBuilder->CreateBr(loopCond);
        irBuilder->SetInsertPoint(loopCond);

        // Ejecutar fscanf
        std::vector<Value*> scanArgs;
        scanArgs.push_back(filePtr);
        scanArgs.push_back(irBuilder->CreateGlobalStringPtr(scanFmt));
        for(auto ptr : readBuffers) scanArgs.push_back(ptr);

        Value* scanResult = irBuilder->CreateCall(fscanfFunc, scanArgs);
        Value* expectedCols = ConstantInt::get(Type::getInt32Ty(context), info.columns.size());
        Value* isSuccess = irBuilder->CreateICmpEQ(scanResult, expectedCols);
        irBuilder->CreateCondBr(isSuccess, loopBody, loopEnd);

        // --- IMPRIMIR FILA CON FORMATO FIJO ---
        irBuilder->SetInsertPoint(loopBody);
        
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("|")}); // Inicio de linea

        for (const auto& item : selectedCols) {
            int idx = item.first;
            DataType dt = item.second.type;
            Value* rawVal = readBuffers[idx]; 

            // AQUÍ ESTÁ LA MAGIA: %-20s (20 espacios, alineado a la izquierda)
            if (dt == VARCHAR_T) {
                irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(" %-20s |"), rawVal});
            } else {
                Value* valLoaded = irBuilder->CreateLoad(getLLVMType(dt), rawVal);
                std::string outFmt = "";
                // Usamos padding fijo de 20 chars
                if (dt == INT_T || dt == BOOLEAN_T) outFmt = " %-20d |";
                else if (dt == DECIMAL_T) outFmt = " %-20.2f |";
                
                irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(outFmt), valLoaded});
            }
        }
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("\n")}); // Fin de linea
        irBuilder->CreateBr(loopCond); 

        // Fin del loop
        irBuilder->SetInsertPoint(loopEnd);
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)}); // Linea final
        irBuilder->CreateCall(fcloseFunc, {filePtr});
        irBuilder->CreateBr(afterSelectBB);

        irBuilder->SetInsertPoint(afterSelectBB);
        return std::any();
    }

    virtual std::any visitDrop(SQLMiniParser::DropContext *ctx) { return std::any(); }
};