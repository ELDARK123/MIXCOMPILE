#include "Utils.h"
#include "CryptoUtils.h"
#include "Flattening.h"
#include "SplitBasicBlock.h"
//#include "llvm/Transforms/Utils/LowerSwitch.h"
// namespace
using namespace llvm;
using std::vector;

#define DEBUG_TYPE "flattening" // 调试标识
// Stats
STATISTIC(Flattened, "Functions flattened");

PreservedAnalyses FlatteningPass::run(Function& F, FunctionAnalysisManager& AM) {
    Function *tmp = &F; // 传入的Function
    // 判断是否需要开启控制流平坦化
    if (toObfuscate(flag, tmp, "fla")) {
      // 用于读取指定的函数元数据
      // 步骤1：通过key "FLA_annotations" 获取MDNode（和设置时的key完全一致）
      MDNode *FLAAnnotMD = (*tmp).getMetadata("FLA_annotations");
      if (FLAAnnotMD) {
        // 步骤2：检查MDNode的操作数数量（你的设置逻辑中只有1个操作数，即MDString）
        if (FLAAnnotMD->getNumOperands() != 1) {
          errs() << "Warning: FLA_annotations metadata has invalid operand count for function " << (*tmp).getName() << "\n";
          return PreservedAnalyses::all();
        }

        // 步骤3：将操作数安全转换为MDString（使用dyn_cast，避免类型不匹配崩溃）
        MDString *FLAAnnotStr = dyn_cast<MDString>(FLAAnnotMD->getOperand(0));
        if (!FLAAnnotStr) {
            errs() << "Warning: FLA_annotations metadata is not a valid MDString for function " << (*tmp).getName() << "\n";
            return PreservedAnalyses::all();
        }

        // 步骤4：转换为C++ std::string
        if(FLAAnnotStr->getString().str() == "nofla") return PreservedAnalyses::all();
      }
      INIT_CONTEXT(F);
      outs()<<"[Soule] debug. "<< F.getName()<<" \n";
      if (flatten(*tmp)) {
        ++Flattened;
      }
      return PreservedAnalyses::none();
    }
    return PreservedAnalyses::all();
}


bool FlatteningPass::flatten(Function &F) {
    // 基本块数量不超过1则无需平坦化
    if(F.size() <= 1){
        //outs() << "\033[0;33mFunction size is lower then one\033[0m\n"; // warning
        return false;
    }
    // emmmm.......
    if (F.getName().str().find("$basic_ostream") != std::string::npos) {
      outs() << "[obf] force_nofla: " << F.getName().str().c_str() << "\n";
      return false;
    }
    // 将除入口块（第一个基本块）以外的基本块保存到一个 vector 容器中，便于后续处理
    // 首先保存所有基本块
    vector<BasicBlock*> origBB;
    for(BasicBlock &BB: F){
        origBB.push_back(&BB);
    }
    // 从vector中去除第一个基本块
    origBB.erase(origBB.begin());
    BasicBlock &entryBB = F.getEntryBlock();
    // 如果第一个基本块的末尾是条件跳转，单独分离
    bool bEntryBB_isConditional = false;
    if(BranchInst *br = dyn_cast<BranchInst>(entryBB.getTerminator())){
        if(br->isConditional()){
            BasicBlock *newBB = entryBB.splitBasicBlock(br, "newBB");
            origBB.insert(origBB.begin(), newBB);
            bEntryBB_isConditional = true;
        }
    }

    // 创建分发块和返回块
    BasicBlock *dispatchBB = BasicBlock::Create(*CONTEXT, "dispatchBB", &F, &entryBB);
    BasicBlock *returnBB = BasicBlock::Create(*CONTEXT, "returnBB", &F, &entryBB);
    BranchInst::Create(dispatchBB, returnBB);
    entryBB.moveBefore(dispatchBB);
    // 去除第一个基本块末尾的跳转
    entryBB.getTerminator()->eraseFromParent();
    // 使第一个基本块跳转到dispatchBB
    BranchInst *brDispatchBB = BranchInst::Create(dispatchBB, &entryBB);

    // 在入口块插入alloca和store指令创建并初始化switch变量，初始值为随机值
    int randNumCase = rand();
    AllocaInst *swVarPtr = new AllocaInst(TYPE_I32, 0, "swVar.ptr", brDispatchBB);
    new StoreInst(CONST_I32(randNumCase), swVarPtr, brDispatchBB);
    // 在分发块插入load指令读取switch变量
    LoadInst *swVar = new LoadInst(TYPE_I32, swVarPtr, "swVar", false, dispatchBB);
    // 在分发块插入switch指令实现基本块的调度
    BasicBlock *swDefault = BasicBlock::Create(*CONTEXT, "swDefault", &F, returnBB);
    BranchInst::Create(returnBB, swDefault);
    SwitchInst *swInst = SwitchInst::Create(swVar, swDefault, 0, dispatchBB);
    // 将原基本块插入到返回块之前，并分配case值
    for(BasicBlock *BB : origBB){
        BB->moveBefore(returnBB);
        swInst->addCase(CONST_I32(randNumCase), BB);
        randNumCase = rand();
    }

    // 在每个基本块最后添加修改switch变量的指令和跳转到返回块的指令
    for(BasicBlock *BB : origBB){
        // retn BB
        if(BB->getTerminator()->getNumSuccessors() == 0){
            continue;
        }
        // 非条件跳转
        else if(BB->getTerminator()->getNumSuccessors() == 1){
            BasicBlock *sucBB = BB->getTerminator()->getSuccessor(0);
            //belong is wrong
            // if (bEntryBB_isConditional) {
            //     entryBB.getTerminator()->eraseFromParent();
            // }
            BB->getTerminator()->eraseFromParent();
            ConstantInt *numCase = swInst->findCaseDest(sucBB);
            new StoreInst(numCase, swVarPtr, BB);
            BranchInst::Create(returnBB, BB);
        }
        // 条件跳转
        else if(BB->getTerminator()->getNumSuccessors() == 2){
            // BranchInst *br = cast<BranchInst>(BB->getTerminator());
            BranchInst *br = dyn_cast<BranchInst>(BB->getTerminator());
            if (!br) {
              //outs() << "[FAILED] dyn_cast<BranchInst>(BB->getTerminator()); " << BB->getName() << "\n";
              continue;
            }
            if (!br->isConditional()) {
              //outs() << "[FAILED] br->isConditional(); " << BB->getName() << "\n";
              continue;
            }
            ConstantInt *numCaseTrue = swInst->findCaseDest(BB->getTerminator()->getSuccessor(0));
            ConstantInt *numCaseFalse = swInst->findCaseDest(BB->getTerminator()->getSuccessor(1));
            SelectInst *sel = SelectInst::Create(br->getCondition(), numCaseTrue, numCaseFalse, "", BB->getTerminator());
            BB->getTerminator()->eraseFromParent();
            new StoreInst(sel, swVarPtr, BB);
            BranchInst::Create(returnBB, BB);
        }
    }
    fixStack(F); // 修复逃逸变量和PHI指令
    return true;
}

FlatteningPass *llvm::createFlattening(bool flag) {
    return new FlatteningPass(flag);
}
