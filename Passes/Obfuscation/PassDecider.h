#ifndef LLVM_PASSDECIDER_H
#define LLVM_PASSDECIDER_H
// LLVM libs
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
// User libs
#include "Utils.h"
#include "CryptoUtils.h"
#include "ObfuscationOptions.h"
using namespace llvm;
using namespace std;
namespace llvm { // 间接跳转
    class PassDecider: public PassInfoMixin<PassDecider>{
        public:
            bool flag;
            PassDecider(bool flag){
                this->flag = flag;
            } // 携带flag的构造函数
            PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM); // Pass实现函数
            static bool isRequired() {return true; } // 直接返回true即可
    };
    PassDecider *createPassDecider(bool flag); // 创建
}
#endif
