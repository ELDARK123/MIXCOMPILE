/*
LLVM Indirect Branching Pass
Copyright (C) 2017 Zhang(https://github.com/Naville/)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "IndirectBranch.h"
#include <random>
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

// Instruction* getFinalDefiningInstruction(Value *V,SmallPtrSet<PHINode*, 4> &Visited,PHINode *Phi,BasicBlock* DestBB0 ,BasicBlock* DestBB1,DominatorTree &DT) {
//   if (!V) return nullptr;

//   // 1. 若 V 是 PHINode，递归处理其入边值（取第一个有效入边，或遍历所有）
//   if (PHINode *PN = dyn_cast<PHINode>(V)) {
//     errs() << "发现嵌套 PHINode：" << *PN << "，递归查找入边值定义指令\n";
//     if (Visited.count(PN)) {
//       errs() << "检测到 Phi 节点闭环引用：" << *PN << "（已访问过，终止递归）\n";
//       return nullptr;
//     }
//     // 标记该 Phi 为已访问，避免重复处理
//     Visited.insert(PN);
//     // 可选：遍历所有入边，或取第一个非 Phi 的入边值
//     for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
//       Value *InVal = PN->getIncomingValue(i);
//       // BasicBlock *InBlock = PN->getIncomingBlock(i); // PHI入值对应的前驱BB
//       // bool InBlock_Domed_DestBB0 = DT.dominates(DT.getNode(DestBB0),DT.getNode(InBlock));
//       // bool InBlock_Domed_DestBB1 = DT.dominates(DT.getNode(DestBB1),DT.getNode(InBlock));
//       // if(InBlock_Domed_DestBB0 || InBlock_Domed_DestBB1 ){
//       //   return Phi;
//       // }
//       Instruction *DefInst = getFinalDefiningInstruction(InVal,Visited,Phi,DestBB0,DestBB1,DT);
//       if (DefInst) return DefInst; // 找到第一个有效定义指令
//     }
//     Visited.erase(PN);
//     return nullptr; // 所有入边都是常量/参数
//   }

//   // 2. 若 V 是 Instruction，直接返回（等价于 getDefiningInstruction()）
//   if (Instruction *Inst = dyn_cast<Instruction>(V)) {
//     return Inst;
//   }

//   // 3. 处理无定义指令的情况（常量/参数等）
//   if (isa<Constant>(V)) {
//     errs() << "Value 是常量：" << *V << "，无定义指令\n";
//   } else if (isa<Argument>(V)) {
//     errs() << "Value 是函数参数：" << *V << "，无定义指令\n";
//   } else {
//     errs() << "Value 类型非 Instruction/PHINode：" << V->getValueID() << "\n";
//   }
//   return nullptr;
// }

// bool processPHINode(PHINode *Phi,BasicBlock* DestBB0 ,BasicBlock* DestBB1,DominatorTree &DT) {
//   if (!Phi) return true;

//   // 1. 获取PHINode所在的基本块
//   BasicBlock *PhiBB = Phi->getParent();
//   if (!PhiBB) return true;

//   errs() << "Processing PHINode: " << *Phi << " in BB: " << PhiBB->getName()<< "\n";

//   // 2. 遍历PHINode的所有入值（[值, 前驱BB] 对）
//   for (unsigned i = 0, e = Phi->getNumIncomingValues(); i < e; ++i) {
//     Value *InVal = Phi->getIncomingValue(i);
//     BasicBlock *InBlock = Phi->getIncomingBlock(i); // PHI入值对应的前驱BB

//     // 3. 跳过常量/参数（无“定义指令”的情况，自然无需检查同块）
//     if (isa<Constant>(InVal) || isa<Argument>(InVal)) {
//       errs() << "  Skip: InVal is Constant/Argument (" << *InVal << ")\n";
//       continue;
//     }

//     // 4. 获取入值的定义指令（SSA中每个值仅有一个定义指令）
//     SmallPtrSet<PHINode*, 4> Visited;
//     Instruction *DefInst = getFinalDefiningInstruction(InVal,Visited,Phi,DestBB0,DestBB1,DT);
//     if (!DefInst) {
//       errs() << "  Skip: InVal has no defining instruction (" << *InVal << ")\n";
//       continue;
//     }

//     // 5. 获取定义指令所在的基本块
//     BasicBlock *DefBB = DefInst->getParent();
//     if (!DefBB) {
//       errs() << " DefInst has no parent BB (" << *DefInst << ")\n";
//       continue;
//     }

//     // 6. 核心判断：定义指令的BB == PHINode的BB → 跳过
//     if (DefBB == PhiBB) {
//       errs() << "  Skip: InVal (" << *InVal << ") is defined in the same BB as PHINode\n";
//       return false;
//     }

//     // 7. 非同一BB的情况：执行你的业务逻辑（如检查支配关系、修正SSA等）
//     errs() << "  Process InVal: " << *InVal << " (defined in BB: " << DefBB->getName() << ")\n";
//     // ========== 这里添加你的处理逻辑 ==========
//     // 例如：检查DefInst是否支配PHINode的入值前驱BB
//     // DominatorTree &DT = ...;
//     // if (DT.dominates(DefInst, InBlock)) { ... }
//   }
//   return true;
// }

// // 辅助函数：判断基本块是否为条件分支块（终止指令是条件分支/switch）
//   bool isCondBranchBlock(BasicBlock *BB, 
//                          std::unordered_map<BasicBlock*, bool> &Cache) {
//     // 查缓存，避免重复计算
//     if (Cache.count(BB)) return Cache[BB];

//     auto *TI = dyn_cast<BranchInst>((*BB).getTerminator());
//     bool IsCond = false;

//     // 条件1：BranchInst且是条件分支（不是unconditional）
//     if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
//       IsCond = BI->isConditional();
//     }
//     // 条件2：SwitchInst（本身就是条件分支）
//     // else if (dyn_cast<SwitchInst>(TI)) {
//     //   IsCond = true;
//     // }

//     Cache[BB] = IsCond;
//     return IsCond;
//   }

//   // 辅助函数：判断两个基本块之间是否可达（DFS遍历CFG）
//   bool isReachable(BasicBlock *FromBB, BasicBlock *ToBB) {
//     SmallPtrSet<BasicBlock*, 16> Visited;
//     SmallVector<BasicBlock*, 16> Worklist;

//     Worklist.push_back(FromBB);
//     Visited.insert(FromBB);

//     while (!Worklist.empty()) {
//       BasicBlock *Current = Worklist.pop_back_val();
//       if (Current == ToBB) return true;

//       // 遍历当前块的所有后继
//       for (BasicBlock *Succ : successors(Current)) {
//         if (!Visited.count(Succ)) {
//           Visited.insert(Succ);
//           Worklist.push_back(Succ);
//         }
//       }
//     }
//     return false;
//   }

//   // 核心函数：判断从FromBB到ToBB的路径上是否存在条件分支
//   bool hasCondBranchOnPath(BasicBlock *FromBB, BasicBlock *ToBB,
//                            std::unordered_map<BasicBlock*, bool> &Cache,
//                           SmallPtrSet<BasicBlock*, 16> &PathBBs,
//                         DenseMap<const BasicBlock*, unsigned> IRBBToIndex) {
//     SmallPtrSet<BasicBlock*, 16> Visited;
//     SmallVector<BasicBlock*, 16> Worklist;

//     Worklist.push_back(FromBB);
//     Visited.insert(FromBB);
//     dbgs()<<"From IR BB is "<<IRBBToIndex.lookup(FromBB)<<"\n";
//     dbgs()<<"To IR BB is "<<IRBBToIndex.lookup(ToBB)<<"\n";
//     bool found_condbranch = false;
    
//     while (!Worklist.empty()) {
//       BasicBlock *Current = Worklist.pop_back_val();

//       // 找到目标块，路径结束（未找到条件分支则返回false）
//       if (Current == ToBB) continue;

//       // 检查当前块是否是条件分支块
//       if (isCondBranchBlock(Current, Cache)) {
//         PathBBs.insert(Current);
//         dbgs()<<"Current IR BB is "<<IRBBToIndex.lookup(Current)<<"\n";
//         found_condbranch  = true;
//       }

//       // 遍历后继，继续DFS
//       for (BasicBlock *Succ : successors(Current)) {
//         if (!Visited.count(Succ) && isReachable(Succ, ToBB)) {
//           Visited.insert(Succ);
//           Worklist.push_back(Succ);
//         }
//       }
//     }

//     return found_condbranch;
//   }


PreservedAnalyses IndirectBranchPass::run(Module &M, ModuleAnalysisManager &AM) {
  if (this->flag) {
    outs() << "[Soule] force.run.IndirectBranchPass\n";
  }
  for (Function &Fn : M) {
    if (toObfuscate(flag, &Fn, "ibr")) {

      if (Options && Options->skipFunction(Fn.getName())) {
        continue;
      }

      if (Fn.empty() || Fn.hasLinkOnceLinkage() || Fn.getSection() == ".text.startup") {
        continue;
      }
      // 用于读取指定的函数元数据
      // 步骤1：通过key "IBR_annotations" 获取MDNode（和设置时的key完全一致）
      MDNode *IBRAnnotMD = Fn.getMetadata("IBR_annotations");
      if (IBRAnnotMD) {
        // 步骤2：检查MDNode的操作数数量（你的设置逻辑中只有1个操作数，即MDString）
        if (IBRAnnotMD->getNumOperands() != 1) {
          errs() << "Warning: IBR_annotations metadata has invalid operand count for function " << Fn.getName() << "\n";
          continue;
        }

        // 步骤3：将操作数安全转换为MDString（使用dyn_cast，避免类型不匹配崩溃）
        MDString *IBRAnnotStr = dyn_cast<MDString>(IBRAnnotMD->getOperand(0));
        if (!IBRAnnotStr) {
          errs() << "Warning: IBR_annotations metadata is not a valid MDString for function " << Fn.getName() << "\n";
          continue;
        }

        // 步骤4：转换为C++ std::string
        if(IBRAnnotStr->getString().str() == "noibr") continue;
      }

      LLVMContext &Ctx = Fn.getContext();

      // Init member fields
      BBNumbering.clear();
      BBTargets.clear();

      // llvm cannot split critical edge from IndirectBrInst
    //   SplitAllCriticalEdges(Fn, CriticalEdgeSplittingOptions(nullptr, nullptr));
      NumberBasicBlock(Fn);

      if (BBNumbering.empty()) {
        continue;
      }

      uint64_t V = RandomEngine.get_uint64_t();
      IntegerType *intType = Type::getInt32Ty(Ctx);
      unsigned pointerSize =
          Fn.getEntryBlock().getModule()->getDataLayout().getTypeAllocSize(
              PointerType::getUnqual(Fn.getContext())); // Soule
      if (pointerSize == 8) {
        intType = Type::getInt64Ty(Ctx);
      }
      ConstantInt *EncKey = ConstantInt::get(intType, V, false);
      ConstantInt *EncKey1 = ConstantInt::get(intType, -V, false);

      Value *MySecret = ConstantInt::get(intType, 0, true);

      ConstantInt *Zero = ConstantInt::get(intType, 0);
      GlobalVariable *DestBBs = getIndirectTargets(Fn, EncKey1);

      for (auto &BB : Fn) {
        auto *BI = dyn_cast<BranchInst>(BB.getTerminator());
        if (BI && BI->isConditional()) {
          IRBuilder<> IRB(BI);

          Value *Cond = BI->getCondition();
          Value *Idx;
          Value *TIdx, *FIdx;

          TIdx = ConstantInt::get(intType, BBNumbering[BI->getSuccessor(0)]);
          FIdx = ConstantInt::get(intType, BBNumbering[BI->getSuccessor(1)]);
          Idx = IRB.CreateSelect(Cond, TIdx, FIdx);

          Value *GEP =
              IRB.CreateGEP(DestBBs->getValueType(), DestBBs, {Zero, Idx});
          Value *EncDestAddr =
              IRB.CreateLoad(GEP->getType(), GEP, "EncDestAddr");
          // -EncKey = X - FuncSecret
          Value *DecKey = IRB.CreateAdd(EncKey, MySecret);
          Value *DestAddr =
              IRB.CreateGEP(Type::getInt8Ty(Ctx), EncDestAddr, DecKey);

          IndirectBrInst *IBI = IndirectBrInst::Create(DestAddr, 2);
          IBI->addDestination(BI->getSuccessor(0));
          IBI->addDestination(BI->getSuccessor(1));
          ReplaceInstWithInst(BI, IBI);
        }
      }
    }
  }
  return PreservedAnalyses::none();
}

// PreservedAnalyses IndirectBranchPass::run(Module &M, ModuleAnalysisManager &AM) {
//   if (this->flag) {
//     outs() << "[Soule] force.run.IndirectBranchPass\n";
//   }
//   for (Function &Fn : M) {
//     if (toObfuscate(flag, &Fn, "ibr")) {

//       if (Options && Options->skipFunction(Fn.getName())) {
//         continue;
//       }

//       if (Fn.empty() || Fn.hasLinkOnceLinkage() || Fn.getSection() == ".text.startup") {
//         continue;
//       }

//       LLVMContext &Ctx = Fn.getContext();

//       // Init member fields
//       BBNumbering.clear();
//       BBTargets.clear();

//       // llvm cannot split critical edge from IndirectBrInst
//       SplitAllCriticalEdges(Fn, CriticalEdgeSplittingOptions(nullptr, nullptr));
//       NumberBasicBlock(Fn); // 保留BB编号（用于数组索引）

//       if (BBNumbering.empty()) {
//         continue;
//       }

//       // ========== 移除所有加密密钥相关代码 ==========
//       // 简化：根据指针大小确定索引类型（仅用于数组索引，无加密）
//       IntegerType *intType;
//       unsigned pointerSize = Fn.getEntryBlock().getModule()->getDataLayout().getTypeAllocSize(
//           PointerType::getUnqual(Fn.getContext()));
//       if (pointerSize == 8) {
//         intType = Type::getInt64Ty(Ctx);
//       } else {
//         intType = Type::getInt32Ty(Ctx);
//       }
//       ConstantInt *Zero = ConstantInt::get(intType, 0);
      
//       // ========== 修改：获取存储原始BB地址的全局变量（无加密） ==========
//       GlobalVariable *DestBBs = getIndirectTargets(Fn); // 不再传递加密密钥EncKey1

//         // 获取支配树分析结果
//       DominatorTree DT(Fn);
//       // 缓存：基本块是否为条件分支块（避免重复判断）
//       std::unordered_map<BasicBlock*, bool> IsCondBranchBlockCache;
//       std::unordered_map<BasicBlock*, bool> IsPHIincomingBlockCache;
//       SmallPtrSet<BasicBlock*, 16> PathBBs;
//       DenseMap<const BasicBlock*, unsigned> IRBBToIndex;
//       unsigned IRBBIndex = 0;
//       for (const BasicBlock &BB : Fn) { // 遍历函数的所有 IR 基本块
//         IRBBToIndex[&BB] = IRBBIndex++;
//       }
//       dbgs()<<"Function name is "<<Fn.getName()<<"\n";
//       for (auto &BB : Fn) {
//         bool token = true;
//         for (Instruction &DefInst : BB) {
//         // 跳过无使用的指令（无需分析）
//           if (DefInst.use_empty()) continue;
//             // 遍历该定义的所有使用点
//           dbgs()<<"Instruction is "<<DefInst<<"\n";
//           for (Use &U : DefInst.uses()) {
//             User *UseUser = U.getUser();
//             Value *UseVal = cast<Value>(UseUser); // 使用点的Value（可能是PHINode/Instruction）
//             // 存储使用点对应的基本块（可能多个，如PHINode的多个前驱块）
//             SmallVector<BasicBlock*, 4> UseBBs;
//             if (PHINode *Phi = dyn_cast<PHINode>(UseUser)) {
//             // 遍历PHINode的所有操作数对（值 + 前驱块）
//               for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
//               // 找到当前变量定义作为PHI操作数的位置
//                 if (Phi->getIncomingValue(i) == &DefInst) {
//                   dbgs()<<"Instruction2 is "<<DefInst<<"\n";
//                 // PHI节点的使用点基本块 = 对应的前驱块（而非PHI所在块）
//                   BasicBlock *IncomingBB = Phi->getIncomingBlock(i);
//                   dbgs()<<"Incoming BB is "<<IRBBToIndex.lookup(IncomingBB)<<"\n";
//                   UseBBs.push_back(IncomingBB);
//                 }
//               }
//             // 若当前变量不是PHI的操作数（只是User），则PHI所在块为使用块
//               if (UseBBs.empty()) {
//                 UseBBs.push_back(Phi->getParent());
//               }
//             }
//             else if (Instruction *UseInst = dyn_cast<Instruction>(UseUser)) {
//               UseBBs.push_back(UseInst->getParent());
//             }
//             // 场景3：其他Value类型（如ConstantExpr，按需扩展）
//             else {
//               // 跳过无法确定基本块的Value（如全局变量/常量）
//               continue;
//             } 
//             for(BasicBlock *UseBB:UseBBs){
//               BasicBlock *DefBB = &BB;          // 定义块
//                dbgs()<<"Use BB is "<<IRBBToIndex.lookup(UseBB)<<"\n";
//               if (DefBB == UseBB){
//                 continue;
//               } 
//               // 条件2：定义块到使用块可达（通过支配树快速判断）
//               if (!DT.isReachableFromEntry(DefBB) || !DT.isReachableFromEntry(UseBB)){
//                 continue;
//               }
//               // 无支配关系，需检查CFG可达性
//               if (!isReachable(DefBB, UseBB)){
//                 continue;
//               }
//               if (isReachable(UseBB, UseBB)){
//                 PathBBs.insert(UseBB);
//               }
//               // 条件3：路径上存在条件branch
//               if (hasCondBranchOnPath(DefBB, UseBB, IsCondBranchBlockCache,PathBBs,IRBBToIndex)) {
//                 // 输出满足条件的变量信息
//                 continue;
//               }
//             }
//           }
//         }

//         // if(token){
//         //   continue;
//         // }

//         if(PathBBs.contains(&BB)){
//           dbgs()<<"Continue IR BB is "<<IRBBToIndex.lookup(&BB)<<"\n";
//           continue;
//         }
          
//         auto *BI = dyn_cast<BranchInst>(BB.getTerminator());
//         if (BI && BI->isConditional()) {
//           dbgs()<<"Conditional BB is "<<IRBBToIndex.lookup(&BB)<<"\n";
//           IRBuilder<> IRB(BI);

//           Value *Cond = BI->getCondition();
//           Value *Idx;
//           Value *TIdx, *FIdx;
//           BasicBlock* DestBB0 = BI->getSuccessor(0);
//           BasicBlock* DestBB1 = BI->getSuccessor(1);

//           // bool token = true;
//           // if (Instruction *FirstInst = BB.getFirstNonPHI()) {
//           //   for (Instruction &I : BB) {
//           //     if (PHINode *Phi = dyn_cast<PHINode>(&I)) {
//           //       token = processPHINode(Phi,DestBB0,DestBB1,DT);
//           //       if(!token){
//           //         break;
//           //       }
//           //       for (unsigned i = 0, e = Phi->getNumIncomingValues(); i < e; ++i) {
//           //         BasicBlock *InBlock = Phi->getIncomingBlock(i);
//           //         SmallPtrSet<BasicBlock*, 8> Visited; // 标记已访问的 BB，防循环
//           //         SmallVector<BasicBlock*, 8> Queue;   // BFS 队列
//           //         // 初始化队列：先加入 BB
//           //         Queue.push_back(&BB);
//           //         Visited.insert(&BB);
//           //         while (!Queue.empty()) {
//           //           BasicBlock *CurrBB = Queue.pop_back_val();
//           //           // 遍历当前 BB 的所有直接后继
//           //           for (BasicBlock *Succ : successors(CurrBB)) {
//           //             if (Succ == InBlock)
//           //             {
//           //               dbgs()<<"Find Succ!\n";
//           //               token=false; // 找到目标，返回可达
//           //               break;
//           //             }
//           //             if (!Visited.count(Succ)) {   // 未访问过，加入队列继续遍历
//           //               Visited.insert(Succ);
//           //               Queue.push_back(Succ);
//           //             }
//           //           }
//           //         } 
//           //         if(!token){
//           //           break;
//           //         }
//           //       }
//           //     } 
//           //   }
//           // } 
//           // if(!token){
//           //   continue;
//           // }

//           // 保留：根据分支条件选择目标BB的编号（无加密）
//           TIdx = ConstantInt::get(intType, BBNumbering[BI->getSuccessor(0)]);
//           FIdx = ConstantInt::get(intType, BBNumbering[BI->getSuccessor(1)]);
//           Idx = IRB.CreateSelect(Cond, TIdx, FIdx);

//           // ========== 移除加密地址加载/解密逻辑，直接获取原始BB地址 ==========
//           // 计算原始BB地址的数组索引（无加密）
//           Value *GEP = IRB.CreateGEP(DestBBs->getValueType(), DestBBs, {Zero, Idx});
//           // 直接加载原始目标地址（无加密）
//           Value *DestAddr = IRB.CreateLoad(GEP->getType(), GEP, "DestAddr");

//           // ========== 保留间接分支创建，但使用原始地址 ==========
//           IndirectBrInst *IBI = IndirectBrInst::Create(DestAddr, 2);
//           IBI->addDestination(BI->getSuccessor(0));
//           IBI->addDestination(BI->getSuccessor(1));
//           ReplaceInstWithInst(BI, IBI);
//         }
//       }
//     }
//   }
//   return PreservedAnalyses::none();
// }

void IndirectBranchPass::NumberBasicBlock(Function &F) {
  for (auto &BB : F) {
    if (auto *BI = dyn_cast<BranchInst>(BB.getTerminator())) {
      if (BI->isConditional()) {
        unsigned N = BI->getNumSuccessors();
        for (unsigned I = 0; I < N; I++) {
          BasicBlock *Succ = BI->getSuccessor(I);
          if (BBNumbering.count(Succ) == 0) {
            BBTargets.push_back(Succ);
            BBNumbering[Succ] = 0;
          }
        }
      }
    }
  }

  long seed = RandomEngine.get_uint32_t();
  std::default_random_engine e(seed);
  std::shuffle(BBTargets.begin(), BBTargets.end(), e);

  unsigned N = 0;
  for (auto BB : BBTargets) {
    BBNumbering[BB] = N++;
  }
}

GlobalVariable *IndirectBranchPass::getIndirectTargets(Function &F, ConstantInt *EncKey) {
  std::string GVName(F.getName().str() + "_IndirectBrTargets");
  GlobalVariable *GV = F.getParent()->getNamedGlobal(GVName);
  if (GV)
    return GV;

  // encrypt branch targets
  std::vector<Constant *> Elements;
  for (const auto BB : BBTargets) {
    Constant *CE = ConstantExpr::getBitCast(BlockAddress::get(BB),
                                            Type::getInt8PtrTy(F.getContext()));
    CE = ConstantExpr::getGetElementPtr(Type::getInt8Ty(F.getContext()), CE,
                                        EncKey);
    Elements.push_back(CE);
  }

  ArrayType *ATy =
      ArrayType::get(Type::getInt8PtrTy(F.getContext()), Elements.size());
  Constant *CA = ConstantArray::get(ATy, ArrayRef<Constant *>(Elements));
  GV =
      new GlobalVariable(*F.getParent(), ATy, false,
                         GlobalValue::LinkageTypes::PrivateLinkage, CA, GVName);
  appendToCompilerUsed(*F.getParent(), {GV});
  return GV;
}

// GlobalVariable *IndirectBranchPass::getIndirectTargets(Function &F) { // 核心修改1：移除EncKey参数
//   std::string GVName(F.getName().str() + "_IndirectBrTargets");
//   GlobalVariable *GV = F.getParent()->getNamedGlobal(GVName);
//   if (GV)
//     return GV;

//   // 核心修改2：移除所有地址加密逻辑，仅收集BB原始地址
//   std::vector<Constant *> Elements;
//   for (const auto BB : BBTargets) {
//     // 仅做类型转换：将BB地址转换为int8*类型的原始指针（无加密偏移）
//     Constant *CE = ConstantExpr::getBitCast(
//         BlockAddress::get(BB), 
//         Type::getInt8PtrTy(F.getContext())
//     );
//     // 核心修改3：删除原GEP加密偏移步骤（CE = ConstantExpr::getGetElementPtr(...)）
//     Elements.push_back(CE);
//   }

//   // 保留数组类型、常量数组创建逻辑（仅存储原始地址）
//   ArrayType *ATy =
//       ArrayType::get(Type::getInt8PtrTy(F.getContext()), Elements.size());
//   Constant *CA = ConstantArray::get(ATy, ArrayRef<Constant *>(Elements));
  
//   // 保留全局变量创建逻辑（属性与原逻辑一致，仅内容为原始地址）
//   GV = new GlobalVariable(
//       *F.getParent(), ATy, false,
//       GlobalValue::LinkageTypes::PrivateLinkage, CA, GVName);
//   appendToCompilerUsed(*F.getParent(), {GV});
//   return GV;
// }

IndirectBranchPass *llvm::createIndirectBranch(bool flag) {
  return new IndirectBranchPass(flag);
}