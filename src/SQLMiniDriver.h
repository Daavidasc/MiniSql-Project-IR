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
#include "SQLMiniParser.h" 

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <fstream> 
#include <sstream> 
#include <algorithm> 

using namespace antlr4;
using namespace llvm;
using namespace std;

enum DataType { INT_T, DECIMAL_T, VARCHAR_T, BOOLEAN_T };

struct ColumnInfo {
    std::string name;
    DataType type;
};

struct QueryColumn {
    int index; 
    ColumnInfo info;
    SQLMiniParser::FunctionCallContext* funcCtx; 
};

struct TableInfo {
    StructType* llvmStructType; 
    std::vector<ColumnInfo> columns;
};

class SQLMiniDriver : public SQLMiniBaseVisitor {
private:
    std::map<std::string, TableInfo> symbolTable; 
    
    // --- VARIABLES DE CONTEXTO (NUEVO) ---
    // Estas variables permiten a compileComparison acceder a los datos de la fila actual
    std::vector<Value*> activeReadBuffers;
    TableInfo* activeTableInfo = nullptr;

    // Helper case-insensitive
    bool iequals(const string& a, const string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (tolower(a[i]) != tolower(b[i])) return false;
        return true;
    }

    DataType getDataTypeFromText(const std::string& typeText) {
        string lowerText = typeText;
        transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
        
        if (lowerText.find("int") != std::string::npos) return INT_T;
        if (lowerText.find("decimal") != std::string::npos) return DECIMAL_T;
        if (lowerText.find("varchar") != std::string::npos) return VARCHAR_T;
        if (lowerText.find("boolean") != std::string::npos) return BOOLEAN_T;
        throw std::runtime_error("Unknown type: " + typeText);
    }

    Type* getLLVMType(DataType dt) {
        switch (dt) {
            case INT_T: return Type::getInt32Ty(context);
            case DECIMAL_T: return Type::getDoubleTy(context);
            case BOOLEAN_T: return Type::getInt32Ty(context); // i32 para evitar corrupción
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
            case VARCHAR_T: return "%s";
            case BOOLEAN_T: return "%d";
        }
        return "";
    }

    std::string getSeparatorLine(int colCount) {
        std::string line = "+";
        for (int i = 0; i < colCount; ++i) {
            line += std::string(22, '-') + "+"; 
        }
        line += "\n";
        return line;
    }

    Value* compileDato(SQLMiniParser::DatoContext *ctx) {
        std::string valText = ctx->getText();

        if (ctx->VINT().size() == 1 && ctx->PTO() == nullptr) { 
            return ConstantInt::get(Type::getInt32Ty(context), std::stoi(valText));
        } else if (ctx->VINT().size() == 2 && ctx->PTO() != nullptr) {
            return ConstantFP::get(Type::getDoubleTy(context), std::stod(valText));
        } else if (ctx->STRING() != nullptr) {
            std::string content = valText.substr(1, valText.size()-2); 
            return irBuilder->CreateGlobalStringPtr(content);
        } else if (ctx->bool_() != nullptr) { 
            bool isTrue = iequals(valText, "true");
            return ConstantInt::get(Type::getInt32Ty(context), isTrue);
        }
        throw std::runtime_error("Unsupported dato type: " + valText);
    }
    
    // --- LÓGICA REAL DE COMPARACIÓN (ACTUALIZADO) ---
    Value* compileComparison(SQLMiniParser::ComparisonContext *ctx) {
        // Verificar que tenemos contexto de lectura
        if (!activeTableInfo || activeReadBuffers.empty()) {
             Value* msg = irBuilder->CreateGlobalStringPtr("[ERROR] Comparison outside SELECT context.\n");
             irBuilder->CreateCall(printfFunc, {msg});
             return ConstantInt::get(Type::getInt1Ty(context), 0);
        }

        // 1. Identificar la columna (Lado Izquierdo)
        std::string colName = ctx->ID()->getText();
        Value* lhsVal = nullptr;
        DataType lhsType;
        
        for(size_t i=0; i < activeTableInfo->columns.size(); ++i) {
            if (activeTableInfo->columns[i].name == colName) {
                // Cargar el valor desde el buffer de lectura actual
                lhsType = activeTableInfo->columns[i].type;
                lhsVal = irBuilder->CreateLoad(getLLVMType(lhsType), activeReadBuffers[i], "lhs_load");
                break;
            }
        }

        if (!lhsVal) {
             std::cerr << "Error: Column " << colName << " not found for comparison." << std::endl;
             return ConstantInt::get(Type::getInt1Ty(context), 0);
        }

        // 2. Obtener el literal (Lado Derecho)
        Value* rhsVal = compileDato(ctx->dato());
        
        // 3. Obtener el operador
        std::string op = "";
        if (ctx->COMP_OP()) op = ctx->COMP_OP()->getText();
        else if (ctx->EQUAL()) op = ctx->EQUAL()->getText();

        // 4. Generar instrucción de comparación según tipos
        // CASO: ENTEROS (INT o BOOLEAN)
        if (lhsType == INT_T || lhsType == BOOLEAN_T) {
            // Si comparamos Int con Decimal, castear Int a Double
            if (rhsVal->getType()->isDoubleTy()) {
                lhsVal = irBuilder->CreateSIToFP(lhsVal, Type::getDoubleTy(context), "cast_i2d");
                // Comparación flotante
                if (op == ">") return irBuilder->CreateFCmpOGT(lhsVal, rhsVal, "cmp_gt");
                if (op == "<") return irBuilder->CreateFCmpOLT(lhsVal, rhsVal, "cmp_lt");
                if (op == ">=") return irBuilder->CreateFCmpOGE(lhsVal, rhsVal, "cmp_ge");
                if (op == "<=") return irBuilder->CreateFCmpOLE(lhsVal, rhsVal, "cmp_le");
                if (op == "=") return irBuilder->CreateFCmpOEQ(lhsVal, rhsVal, "cmp_eq");
                if (op == "!=") return irBuilder->CreateFCmpONE(lhsVal, rhsVal, "cmp_ne");
            } else {
                // Comparación entera normal
                if (op == ">") return irBuilder->CreateICmpSGT(lhsVal, rhsVal, "cmp_gt");
                if (op == "<") return irBuilder->CreateICmpSLT(lhsVal, rhsVal, "cmp_lt");
                if (op == ">=") return irBuilder->CreateICmpSGE(lhsVal, rhsVal, "cmp_ge");
                if (op == "<=") return irBuilder->CreateICmpSLE(lhsVal, rhsVal, "cmp_le");
                if (op == "=") return irBuilder->CreateICmpEQ(lhsVal, rhsVal, "cmp_eq");
                if (op == "!=") return irBuilder->CreateICmpNE(lhsVal, rhsVal, "cmp_ne");
            }
        }
        // CASO: DECIMALES
        else if (lhsType == DECIMAL_T) {
            // Si el literal es entero, castear a double
            if (rhsVal->getType()->isIntegerTy()) {
                rhsVal = irBuilder->CreateSIToFP(rhsVal, Type::getDoubleTy(context), "cast_lit_i2d");
            }
            
            if (op == ">") return irBuilder->CreateFCmpOGT(lhsVal, rhsVal, "cmp_gt");
            if (op == "<") return irBuilder->CreateFCmpOLT(lhsVal, rhsVal, "cmp_lt");
            if (op == ">=") return irBuilder->CreateFCmpOGE(lhsVal, rhsVal, "cmp_ge");
            if (op == "<=") return irBuilder->CreateFCmpOLE(lhsVal, rhsVal, "cmp_le");
            if (op == "=") return irBuilder->CreateFCmpOEQ(lhsVal, rhsVal, "cmp_eq");
            if (op == "!=") return irBuilder->CreateFCmpONE(lhsVal, rhsVal, "cmp_ne");
        }

        // Placeholder para String u otros no soportados
        return ConstantInt::get(Type::getInt1Ty(context), 0); 
    }

    void saveSchema(const std::string& tableName, const std::vector<ColumnInfo>& cols) {
        std::ofstream schemaFile("schema.txt", std::ios::app);
        if (schemaFile.is_open()) {
            schemaFile << tableName;
            for (const auto& col : cols) {
                schemaFile << " " << col.name << ":" << (int)col.type;
            }
            schemaFile << "\n";
            schemaFile.close();
        }
    }

    void loadSchema() {
        std::ifstream schemaFile("schema.txt");
        std::string line;
        while (std::getline(schemaFile, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string tableName, colToken;
            ss >> tableName;
            if (symbolTable.count(tableName)) continue; 

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
        if (symbolTable.count(tableName)) {
            Value* msg = irBuilder->CreateGlobalStringPtr("Table " + tableName + " already exists.\n");
            irBuilder->CreateCall(printfFunc, {msg});
            return std::any();
        }
        std::vector<Type*> structFields;
        std::vector<ColumnInfo> cols;
        StructType* tableStruct = StructType::create(context, "struct." + tableName);
        
        for (auto const& colDef : ctx->columnDefinition()) {
             std::string name = colDef->ID()->getText();
             std::string typeText = colDef->tipoDato()->getText();
            DataType dt = getDataTypeFromText(typeText);
            structFields.push_back(getLLVMType(dt));
            cols.push_back({name, dt});
        }
        tableStruct->setBody(structFields);
        symbolTable[tableName] = {tableStruct, cols};
        saveSchema(tableName, cols);
        Value* msg = irBuilder->CreateGlobalStringPtr("Table '" + tableName + "' created.\n");
        irBuilder->CreateCall(printfFunc, {msg});
        return std::any();
    }

    virtual std::any visitInsert(SQLMiniParser::InsertContext *ctx) {
        std::string tableName = ctx->ID()->getText();
        if (symbolTable.find(tableName) == symbolTable.end()) {
            std::cerr << "Error: Table " << tableName << " not found." << std::endl;
            return std::any();
        }
        TableInfo& info = symbolTable[tableName];
        
        // Safety Check
        auto values = ctx->dato();
        if (values.size() != info.columns.size()) {
             std::cerr << "Insert Error: Column mismatch." << std::endl;
             return std::any();
        }

        Value* filename = irBuilder->CreateGlobalStringPtr(tableName + ".txt");
        Value* mode = irBuilder->CreateGlobalStringPtr("a"); 
        Value* filePtr = irBuilder->CreateCall(fopenFunc, {filename, mode});

        std::string fileFormatStr = "";
        std::vector<Value*> args;
        args.push_back(filePtr); 

        for (size_t i = 0; i < values.size(); ++i) {
            std::string valText = values[i]->getText();
            DataType dt = info.columns[i].type;
            if (i > 0) fileFormatStr += "\t";
            fileFormatStr += getPrintFormat(dt);

            Value* valToStore = nullptr;
            if (dt == INT_T) valToStore = ConstantInt::get(Type::getInt32Ty(context), std::stoi(valText));
            else if (dt == DECIMAL_T) valToStore = ConstantFP::get(Type::getDoubleTy(context), std::stod(valText));
            else if (dt == BOOLEAN_T) {
                bool isTrue = iequals(valText, "true");
                valToStore = ConstantInt::get(Type::getInt32Ty(context), isTrue);
            }
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
        std::string tableName = ctx->ID()->getText();
        if (symbolTable.find(tableName) == symbolTable.end()) {
            Value* errMsg = irBuilder->CreateGlobalStringPtr("Error: Table " + tableName + " not found.\n");
            irBuilder->CreateCall(printfFunc, {errMsg});
            return std::any();
        }
        
        TableInfo& info = symbolTable[tableName];
        std::vector<QueryColumn> selectedCols;

        if (ctx->ESTRELLA()) {
            for (size_t i = 0; i < info.columns.size(); ++i) 
                selectedCols.push_back({(int)i, info.columns[i], nullptr});
        } else {
            auto selectedExpressions = ctx->expr();
            for (auto const& exprCtx : selectedExpressions) {
                if (exprCtx->ID() != nullptr) { 
                    std::string colName = exprCtx->ID()->getText();
                    for(size_t j=0; j<info.columns.size(); ++j) {
                        if(info.columns[j].name == colName) {
                            selectedCols.push_back({(int)j, info.columns[j], nullptr});
                            break;
                        }
                    }
                } else if (exprCtx->functionCall() != nullptr) { 
                    ColumnInfo calculatedCol = {"Calculated_IF", VARCHAR_T};
                    if (exprCtx->functionCall()->ID() != nullptr) {
                        calculatedCol.name = exprCtx->functionCall()->ID()->getText();
                    }
                    selectedCols.push_back({-1, calculatedCol, exprCtx->functionCall()}); 
                }
            }
        }

        std::string separator = getSeparatorLine(selectedCols.size());
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("\nQuery result for: " + tableName + "\n")});
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});

        std::string headerStr = "|";
        for (const auto& item : selectedCols) {
            std::string name = item.info.name;
            if (name.length() > 20) name = name.substr(0, 20); 
            else name.append(20 - name.length(), ' '); 
            headerStr += " " + name + " |"; 
        }
        headerStr += "\n";
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(headerStr)});
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});

        Value* filename = irBuilder->CreateGlobalStringPtr(tableName + ".txt");
        Value* mode = irBuilder->CreateGlobalStringPtr("r");
        Value* filePtr = irBuilder->CreateCall(fopenFunc, {filename, mode});

        BasicBlock* fileOkBB = BasicBlock::Create(context, "file_ok", mainFunc);
        BasicBlock* fileErrBB = BasicBlock::Create(context, "file_err", mainFunc);
        BasicBlock* afterSelectBB = BasicBlock::Create(context, "after_select", mainFunc);

        Value* isNull = irBuilder->CreateICmpEQ(filePtr, Constant::getNullValue(filePtr->getType()));
        irBuilder->CreateCondBr(isNull, fileErrBB, fileOkBB);

        irBuilder->SetInsertPoint(fileErrBB);
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("| No data found (Empty table)                  |\n")});
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)});
        irBuilder->CreateBr(afterSelectBB);

        irBuilder->SetInsertPoint(fileOkBB);
        std::vector<Value*> readBuffers;
        std::string scanFmt = "";

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

        std::vector<Value*> scanArgs;
        scanArgs.push_back(filePtr);
        scanArgs.push_back(irBuilder->CreateGlobalStringPtr(scanFmt));
        for(auto ptr : readBuffers) scanArgs.push_back(ptr);

        Value* scanResult = irBuilder->CreateCall(fscanfFunc, scanArgs);
        Value* expectedCols = ConstantInt::get(Type::getInt32Ty(context), info.columns.size());
        Value* isSuccess = irBuilder->CreateICmpEQ(scanResult, expectedCols);
        irBuilder->CreateCondBr(isSuccess, loopBody, loopEnd);

        irBuilder->SetInsertPoint(loopBody);
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("|")}); 

        // --- CONTEXTO ACTIVO: Aquí pasamos los buffers a compileComparison ---
        this->activeReadBuffers = readBuffers;
        this->activeTableInfo = &info;

        for (const auto& item : selectedCols) {
            int idx = item.index;
            DataType dt = item.info.type;
            
            if (idx == -1 && item.funcCtx != nullptr) {
                // Ahora visitFunctionCall tendrá acceso a 'activeReadBuffers'
                std::any resultAny = visitFunctionCall(item.funcCtx);
                Value* resultVal = std::any_cast<Value*>(resultAny);
                irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(" %-20s |"), resultVal});
            } else {
                Value* rawVal = readBuffers[idx]; 
                if (dt == VARCHAR_T) {
                    irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(" %-20s |"), rawVal});
                } else {
                    Value* valLoaded = irBuilder->CreateLoad(getLLVMType(dt), rawVal);
                    std::string outFmt = "";
                    if (dt == INT_T || dt == BOOLEAN_T) outFmt = " %-20d |";
                    else if (dt == DECIMAL_T) outFmt = " %-20.2f |";
                    irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(outFmt), valLoaded});
                }
            }
        }
        
        // --- LIMPIEZA DE CONTEXTO ---
        this->activeReadBuffers.clear();
        this->activeTableInfo = nullptr;

        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr("\n")}); 
        irBuilder->CreateBr(loopCond); 

        irBuilder->SetInsertPoint(loopEnd);
        irBuilder->CreateCall(printfFunc, {irBuilder->CreateGlobalStringPtr(separator)}); 
        irBuilder->CreateCall(fcloseFunc, {filePtr});
        irBuilder->CreateBr(afterSelectBB);

        irBuilder->SetInsertPoint(afterSelectBB);
        return std::any();
    }
    
    virtual std::any visitFunctionCall(SQLMiniParser::FunctionCallContext *ctx) override {
        if (!ctx->IF_FUNC()) return std::any(); 

        Value* condition = compileComparison(ctx->comparison()); 
        Value* trueVal = compileDato(ctx->dato(0)); 
        Value* falseVal = compileDato(ctx->dato(1)); 
        
        Function *currentFunc = irBuilder->GetInsertBlock()->getParent();
        BasicBlock *trueBB = BasicBlock::Create(context, "if_true", currentFunc);
        BasicBlock *falseBB = BasicBlock::Create(context, "if_false", currentFunc);
        BasicBlock *mergeBB = BasicBlock::Create(context, "if_merge", currentFunc);

        irBuilder->CreateCondBr(condition, trueBB, falseBB);

        irBuilder->SetInsertPoint(trueBB);
        trueBB = irBuilder->GetInsertBlock();
        irBuilder->CreateBr(mergeBB);

        irBuilder->SetInsertPoint(falseBB);
        falseBB = irBuilder->GetInsertBlock(); 
        irBuilder->CreateBr(mergeBB);

        irBuilder->SetInsertPoint(mergeBB);
        PHINode *phi = irBuilder->CreatePHI(trueVal->getType(), 2, "if_result");
        phi->addIncoming(trueVal, trueBB);
        phi->addIncoming(falseVal, falseBB);

        return (Value*)phi;
    }

    virtual std::any visitDrop(SQLMiniParser::DropContext *ctx) { return std::any(); }

    virtual std::any visitForLoop(SQLMiniParser::ForLoopContext *ctx) override {
        std::string varName = ctx->ID()->getText();
        if (ctx->VINT().size() < 2) throw std::runtime_error("FOR loop error");
        
        int startVal = std::stoi(ctx->VINT(0)->getText());
        int endVal = std::stoi(ctx->VINT(1)->getText());

        Function *currentFunc = irBuilder->GetInsertBlock()->getParent();
        Value* loopVar = irBuilder->CreateAlloca(Type::getInt32Ty(context), nullptr, varName);
        Value* startConstant = ConstantInt::get(Type::getInt32Ty(context), startVal);
        irBuilder->CreateStore(startConstant, loopVar);

        BasicBlock *loopCond = BasicBlock::Create(context, "for_cond", currentFunc);
        BasicBlock *loopBody = BasicBlock::Create(context, "for_body", currentFunc);
        BasicBlock *loopExit = BasicBlock::Create(context, "for_exit", currentFunc);

        irBuilder->CreateBr(loopCond); 
        
        irBuilder->SetInsertPoint(loopCond);
        Value* currentVal = irBuilder->CreateLoad(Type::getInt32Ty(context), loopVar);
        Value* endConstant = ConstantInt::get(Type::getInt32Ty(context), endVal);
        Value* condition = irBuilder->CreateICmpSLE(currentVal, endConstant, "loop_cond_val");
        irBuilder->CreateCondBr(condition, loopBody, loopExit);

        irBuilder->SetInsertPoint(loopBody);
        this->visitInsert(ctx->insert()); 

        Value* stepVal = ConstantInt::get(Type::getInt32Ty(context), 1);
        Value* nextVal = irBuilder->CreateAdd(currentVal, stepVal, "next_i");
        irBuilder->CreateStore(nextVal, loopVar);
        irBuilder->CreateBr(loopCond); 

        irBuilder->SetInsertPoint(loopExit);
        return std::any();
    }
};