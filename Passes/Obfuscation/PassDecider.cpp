#include "PassDecider.h"
#include "llvm/IR/Function.h"
#include "BogusControlFlow.h"
#include "Utils.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

void add_annotation(Function &Fn, const char *annotation,
                    const char *annotation_name);
void add_annotation_bb(BasicBlock &BB, const char *annotation,
                       const char *annotation_name);
bool choose_machine();
static llvm::cl::opt<std::string> MixProfile(
    "mix-profile",
    llvm::cl::desc(
        "MIXCOMPILE profile: balanced, optimized, security, performance"),
    llvm::cl::init("balanced"));

static llvm::cl::opt<double>
    MixScoreThreshold("mix-score-threshold",
                      llvm::cl::desc("Minimum score required to enable a pass"),
                      llvm::cl::init(0.0));

static llvm::cl::opt<std::string>
    MixMode("mix-mode",
            llvm::cl::desc("MIXCOMPILE mode: cost, rule, random, all"),
            llvm::cl::init("cost"));

static llvm::cl::opt<double>
    MixSecurityScale("mix-security-scale",
                     llvm::cl::desc("Scale factor for SecurityGain dimension (default: 1.0)"),
                     llvm::cl::init(1.0));

static llvm::cl::opt<double>
    MixDiversityScale("mix-diversity-scale",
                      llvm::cl::desc("Scale factor for DiversityGain dimension (default: 1.0)"),
                      llvm::cl::init(1.0));

static llvm::cl::opt<double>
    MixRuntimeScale("mix-runtime-scale",
                    llvm::cl::desc("Scale factor for RuntimeCost dimension (default: 1.0)"),
                    llvm::cl::init(1.0));

static llvm::cl::opt<double>
    MixSizeScale("mix-size-scale",
                 llvm::cl::desc("Scale factor for SizeCost dimension (default: 1.0)"),
                 llvm::cl::init(1.0));

static llvm::cl::opt<double>
    MixRiskScale("mix-risk-scale",
                 llvm::cl::desc("Scale factor for CorrectnessRisk dimension (default: 1.0)"),
                 llvm::cl::init(1.0));

static llvm::cl::opt<std::string>
    MixOnlyPass("mix-only-pass",
                llvm::cl::desc("Only allow one MIXCOMPILE pass (SUB, BCF, FLA, SPLIT, IBR, ICALL, IGV). "
                               "Empty string (default) means all passes are allowed."),
                llvm::cl::init(""));

static llvm::cl::opt<std::string>
    MixCostConfig("mix-cost-config",
                  llvm::cl::desc("Path to JSON file with pass base weights."),
                  llvm::cl::init(""));

static llvm::cl::opt<std::string>
    MixDumpCostConfig("mix-dump-cost-config",
                      llvm::cl::desc("Path to write the actually-used pass weights as JSON."),
                      llvm::cl::init(""));

static llvm::cl::opt<std::string>
    MixDisableDimension("mix-disable-dimension",
                        llvm::cl::desc("Disable one dimension: security, diversity, runtime, size, risk."),
                        llvm::cl::init(""));

static llvm::cl::opt<unsigned> MixBCFMaxInstructions(
    "mix-bcf-max-instructions",
    llvm::cl::desc(
        "Maximum function instruction count eligible for BCF (default: 2000)"),
    llvm::cl::init(2000));

static llvm::cl::opt<double> MixBCFMaxExpectedModifiedBB(
    "mix-bcf-max-expected-modified-bb",
    llvm::cl::desc(
        "Maximum estimated modified basic blocks eligible for BCF (default: 500)"),
    llvm::cl::init(500.0));

namespace {

enum class ObfPass { IBR, BCF, FLA, SUB, SPLIT, ICALL, IGV };

struct CostModelWeights {
  double Security;
  double Diversity;
  double Runtime;
  double Size;
  double Risk;
};

struct PassBaseWeight {
  double SecurityGain;
  double DiversityGain;
  double RuntimeCost;
  double SizeCost;
  double CorrectnessRisk;
};

struct PassScore {
  double SecurityGain = 0.0;
  double DiversityGain = 0.0;
  double RuntimeCost = 0.0;
  double SizeCost = 0.0;
  double CorrectnessRisk = 0.0;

  double total(const CostModelWeights &W) const {
    return W.Security * SecurityGain + W.Diversity * DiversityGain -
           W.Runtime * RuntimeCost - W.Size * SizeCost -
           W.Risk * CorrectnessRisk;
  }
};

static double safeRatio(unsigned Numerator, unsigned Denominator) {
  return Denominator == 0 ? 0.0
                          : static_cast<double>(Numerator) /
                                static_cast<double>(Denominator);
}

static double clamp01(double Value) {
  if (Value < 0.0)
    return 0.0;
  if (Value > 1.0)
    return 1.0;
  return Value;
}

static std::string normalizeOptionToken(StringRef Value) {
  return Value.trim().lower();
}

static std::set<std::string> parseDisabledDimensions() {
  std::set<std::string> Disabled;
  std::stringstream Stream(MixDisableDimension.getValue());
  std::string Raw;
  while (std::getline(Stream, Raw, ',')) {
    std::string Token = normalizeOptionToken(Raw);
    if (Token.empty())
      continue;
    if (Token != "security" && Token != "diversity" && Token != "runtime" &&
        Token != "size" && Token != "risk")
      report_fatal_error(Twine("Unknown MIXCOMPILE dimension: ") + Token);
    Disabled.insert(Token);
  }
  return Disabled;
}

static void validateCommandLineOptions() {
  const std::string Profile = normalizeOptionToken(MixProfile.getValue());
  if (Profile != "balanced" && Profile != "optimized" &&
      Profile != "security" && Profile != "performance")
    report_fatal_error(Twine("Unknown MIXCOMPILE profile: ") + Profile);

  const std::string Mode = normalizeOptionToken(MixMode.getValue());
  if (Mode != "cost" && Mode != "rule" && Mode != "random" && Mode != "all")
    report_fatal_error(Twine("Unknown MIXCOMPILE mode: ") + Mode);

  if (!MixOnlyPass.empty()) {
    const std::string Only = normalizeOptionToken(MixOnlyPass.getValue());
    if (Only != "sub" && Only != "bcf" && Only != "fla" &&
        Only != "split" && Only != "ibr" && Only != "icall" &&
        Only != "igv")
      report_fatal_error(Twine("Unknown MIXCOMPILE only-pass: ") + Only);
  }

  for (double Scale : {MixSecurityScale.getValue(), MixDiversityScale.getValue(),
                       MixRuntimeScale.getValue(), MixSizeScale.getValue(),
                       MixRiskScale.getValue()}) {
    if (!std::isfinite(Scale) || Scale < 0.0)
      report_fatal_error("MIXCOMPILE dimension scales must be finite and non-negative");
  }
  if (!std::isfinite(MixBCFMaxExpectedModifiedBB.getValue()) ||
      MixBCFMaxExpectedModifiedBB.getValue() < 0.0)
    report_fatal_error(
        "MIXCOMPILE BCF expected-modified-BB budget must be finite and non-negative");
  (void)parseDisabledDimensions();
}

// Global map for JSON-loaded pass weights (pass_name -> weight struct).
// Populated once on first access via loadCostConfig().
static std::map<std::string, PassBaseWeight> LoadedPassWeights;

static StringRef passToString(ObfPass P) {
  switch (P) {
    case ObfPass::SUB:   return "SUB";
    case ObfPass::BCF:   return "BCF";
    case ObfPass::FLA:   return "FLA";
    case ObfPass::SPLIT: return "SPLIT";
    case ObfPass::IBR:   return "IBR";
    case ObfPass::ICALL: return "ICALL";
    case ObfPass::IGV:   return "IGV";
  }
  llvm_unreachable("unknown pass");
}

static void loadCostConfig() {
  if (MixCostConfig.empty() || !LoadedPassWeights.empty())
    return;
  auto MB = MemoryBuffer::getFile(MixCostConfig);
  if (!MB) {
    report_fatal_error(Twine("Cannot open MIXCOMPILE cost config: ") +
                       MixCostConfig.getValue());
  }
  auto Parsed = json::parse(MB.get()->getBuffer());
  if (!Parsed) {
    report_fatal_error(Twine("Invalid MIXCOMPILE cost config JSON: ") +
                       toString(Parsed.takeError()));
  }
  auto *Root = Parsed->getAsObject();
  if (!Root) {
    report_fatal_error("MIXCOMPILE cost config root must be an object");
  }
  for (auto &[Key, Val] : *Root) {
    auto *Obj = Val.getAsObject();
    if (!Obj) continue;
    auto Sec  = Obj->getNumber("SecurityGain");
    auto Div  = Obj->getNumber("DiversityGain");
    auto Run  = Obj->getNumber("RuntimeCost");
    auto Sz   = Obj->getNumber("SizeCost");
    auto Risk = Obj->getNumber("CorrectnessRisk");
    if (!(Sec && Div && Run && Sz && Risk))
      report_fatal_error(Twine("Incomplete MIXCOMPILE weights for pass ") +
                         Key.str());
    if (!std::isfinite(*Sec) || !std::isfinite(*Div) || !std::isfinite(*Run) ||
        !std::isfinite(*Sz) || !std::isfinite(*Risk) || *Sec < 0.0 ||
        *Sec > 1.0 || *Div < 0.0 || *Div > 1.0 || *Run < 0.0 ||
        *Run > 1.0 || *Sz < 0.0 || *Sz > 1.0 || *Risk < 0.0 ||
        *Risk > 1.0)
      report_fatal_error(
          Twine("MIXCOMPILE weights must be finite values in [0,1] for pass ") +
          Key.str());
    {
      std::string UpperKey = Key.str();
      for (auto &C : UpperKey) C = (C >= 'a' && C <= 'z') ? C - 'a' + 'A' : C;
      PassBaseWeight WB;
      WB.SecurityGain = *Sec;
      WB.DiversityGain = *Div;
      WB.RuntimeCost = *Run;
      WB.SizeCost = *Sz;
      WB.CorrectnessRisk = *Risk;
      LoadedPassWeights[UpperKey] = WB;
      errs() << "[MIXCOMPILE] Loaded weights for " << UpperKey << ": "
             << "S=" << *Sec << " D=" << *Div << " R=" << *Run
             << " Z=" << *Sz << " C=" << *Risk << "\n";
    }
  }
}

static bool isSubstitutableOpcode(unsigned Opcode) {
  switch (Opcode) {
  case Instruction::Add:
  case Instruction::Sub:
  case Instruction::And:
  case Instruction::Or:
  case Instruction::Xor:
    return true;
  default:
    return false;
  }
}

static PassBaseWeight getBaseWeight(ObfPass P) {
  // Check if a JSON cost config was loaded
  if (!LoadedPassWeights.empty()) {
    auto It = LoadedPassWeights.find(passToString(P).str());
    if (It != LoadedPassWeights.end())
      return It->second;
  }
  switch (P) {
  case ObfPass::SUB:
    return {0.75, 0.55, 0.25, 0.05, 0.10};
  case ObfPass::BCF:
    return {0.45, 0.65, 0.20, 0.10, 0.20};
  case ObfPass::FLA:
    return {0.80, 0.75, 0.85, 0.60, 0.35};
  case ObfPass::SPLIT:
    return {0.30, 0.40, 0.15, 0.25, 0.10};
  case ObfPass::IBR:
    return {0.70, 0.65, 0.45, 0.30, 0.45};
  case ObfPass::ICALL:
    return {0.55, 0.50, 0.35, 0.20, 0.30};
  case ObfPass::IGV:
    return {0.55, 0.50, 0.30, 0.35, 0.50};
  }

  return {0.0, 0.0, 1.0, 1.0, 1.0};
}

struct BasicBlockCostFeatures {
  unsigned BBSize = 0;
  unsigned SubInsts = 0;
  double SubRatio = 0.0;
  bool HasCall = false;
  bool HasInlineAsm = false;
};

struct FunctionCostFeatures {
  unsigned NumBBs = 0;
  unsigned NumEdges = 0;
  unsigned NumCondJumps = 0;
  unsigned NumTotalJumps = 0;
  unsigned CyclomaticComplexity = 0;
  double CondJumpRatio = 0.0;
  double AvgCFGDepth = 0.0;
  unsigned TotalInsts = 0;
  unsigned TotalSubInsts = 0;
  unsigned MaxBBSize = 0;
  double SubRatio = 0.0;
  bool HasCall = false;
  bool HasInlineAsm = false;
  bool IBRCompatible = true;
};

struct BCFWorkEstimate {
  int Probability = 0;
  int LoopCount = 0;
  double ExpectedModifiedBB = 0.0;
  double RuntimeMultiplier = 1.0;
  double SizeMultiplier = 1.0;
};

static BCFWorkEstimate estimateBCFWork(const FunctionCostFeatures &F) {
  BCFWorkEstimate Work;
  Work.Probability = getBCFProbability();
  Work.LoopCount = getBCFLoopCount();

  const double Probability =
      clamp01(static_cast<double>(Work.Probability) / 100.0);
  const int LoopCount = std::max(0, Work.LoopCount);
  double ExpansionSum = 0.0;
  double Expansion = 1.0;
  for (int I = 0; I < LoopCount; ++I) {
    ExpansionSum += Expansion;
    Expansion *= 1.0 + 3.0 * Probability;
  }

  Work.ExpectedModifiedBB =
      static_cast<double>(F.NumBBs) * Probability * ExpansionSum;
  const double RuntimeWork = Work.ExpectedModifiedBB * 18.0;
  Work.RuntimeMultiplier = 1.0 + std::log2(1.0 + RuntimeWork) / 10.0;

  const double AvgBBInsts = safeRatio(F.TotalInsts, F.NumBBs);
  const double EstimatedAddedInsts =
      Work.ExpectedModifiedBB * (AvgBBInsts + 18.0);
  Work.SizeMultiplier =
      1.0 + std::log2(1.0 + EstimatedAddedInsts) / 12.0;
  return Work;
}

static void dumpCostConfig() {
  if (MixDumpCostConfig.empty())
    return;
  std::error_code EC;
  llvm::raw_fd_ostream OS(MixDumpCostConfig, EC);
  if (EC) {
    errs() << "[MIXCOMPILE] Cannot open dump file: " << MixDumpCostConfig << "\n";
    return;
  }
  OS << "{\n";
  bool First = true;
  for (ObfPass P : {ObfPass::SUB, ObfPass::BCF, ObfPass::FLA, ObfPass::SPLIT,
                    ObfPass::IBR, ObfPass::ICALL, ObfPass::IGV}) {
    auto W = getBaseWeight(P);
    if (!First) OS << ",\n";
    First = false;
    OS << "  \"" << passToString(P) << "\": {\n"
       << "    \"SecurityGain\": " << W.SecurityGain << ",\n"
       << "    \"DiversityGain\": " << W.DiversityGain << ",\n"
       << "    \"RuntimeCost\": " << W.RuntimeCost << ",\n"
       << "    \"SizeCost\": " << W.SizeCost << ",\n"
       << "    \"CorrectnessRisk\": " << W.CorrectnessRisk << "\n"
       << "  }";
  }
  OS << "\n}\n";
  errs() << "[MIXCOMPILE] Dumped cost config to " << MixDumpCostConfig << "\n";
}

static CostModelWeights getProfileWeights(const std::string &Profile) {
  CostModelWeights W;
  if (Profile == "security")
    W = {1.40, 0.70, 0.55, 0.35, 0.90};
  else if (Profile == "optimized")
    W = {1.10, 1.05, 0.90, 0.25, 0.95};
  else if (Profile == "performance")
    W = {0.80, 0.35, 1.40, 0.90, 1.20};
  else
    W = {1.00, 0.50, 0.80, 0.50, 1.00};

  const std::set<std::string> Disabled = parseDisabledDimensions();
  if (Disabled.count("security"))  W.Security = 0.0;
  if (Disabled.count("diversity")) W.Diversity = 0.0;
  if (Disabled.count("runtime"))   W.Runtime = 0.0;
  if (Disabled.count("size"))      W.Size = 0.0;
  if (Disabled.count("risk"))      W.Risk = 0.0;

  return W;
}

static PassScore estimateFunctionPassScore(ObfPass P,
                                           const FunctionCostFeatures &F) {
  PassBaseWeight B = getBaseWeight(P);
  PassScore S{B.SecurityGain, B.DiversityGain, B.RuntimeCost, B.SizeCost,
              B.CorrectnessRisk};

  switch (P) {
  case ObfPass::IBR:
    S.SecurityGain *= (F.CondJumpRatio >= 0) ? ( 1.0 + F.CondJumpRatio) : 0.0;
    S.DiversityGain *= (F.CondJumpRatio >= 0) ? 1.0 : 0.0;
    S.RuntimeCost *= (F.CondJumpRatio >= 0) ? (1.0 + F.CondJumpRatio) : 0.0;
    S.SizeCost *= (F.CondJumpRatio >= 0) ? 1.0 : 0.0;
    if (!F.IBRCompatible)
      S.CorrectnessRisk *= 1.0;
    else
      S.CorrectnessRisk *= 0.0;
    break;
  case ObfPass::BCF:
    {
      const double Factor = clamp01((static_cast<double>(F.CyclomaticComplexity) - 2.0) / 4.0);
      const BCFWorkEstimate Work = estimateBCFWork(F);
      S.SecurityGain *= Factor;
      S.DiversityGain *= Factor;
      S.RuntimeCost *= Factor * Work.RuntimeMultiplier;
      S.SizeCost *= Factor * (1.0 + F.CondJumpRatio) * Work.SizeMultiplier;
      S.CorrectnessRisk *= Factor;
    }
    break;
  case ObfPass::FLA:
    {
      const double Factor = clamp01(F.AvgCFGDepth / 3.0);
      const double ComplexityRuntimeFactor =
          1.0 + clamp01(static_cast<double>(F.CyclomaticComplexity) / 6.0);
      S.SecurityGain *= Factor;
      S.DiversityGain *= Factor;
      S.RuntimeCost *= Factor * ComplexityRuntimeFactor;
      S.SizeCost *= Factor;
      S.CorrectnessRisk *= Factor;
    }
    break;
  case ObfPass::SPLIT:
    {
      const double Factor = clamp01((static_cast<double>(F.MaxBBSize) - 4.0) / 16.0);
      S.SecurityGain *= Factor;
      S.DiversityGain *= Factor;
      S.RuntimeCost *= Factor;
      S.SizeCost *= Factor;
      S.CorrectnessRisk *= Factor;
    }
    break;
  case ObfPass::IGV:
    S.SecurityGain *= F.HasInlineAsm ? 0.0 : 1.0;
    S.DiversityGain *= F.HasInlineAsm ? 0.0 : 1.0;
    S.RuntimeCost *= F.HasInlineAsm ? 0.0 : 1.0;
    S.SizeCost *= F.HasInlineAsm ? 0.0 : 1.0;
    S.CorrectnessRisk *= F.HasInlineAsm ? 0.0 : 1.0;
    break;
  case ObfPass::SUB:
  case ObfPass::ICALL:
    break;
  }

  S.SecurityGain = clamp01(S.SecurityGain * MixSecurityScale);
  S.DiversityGain = clamp01(S.DiversityGain * MixDiversityScale);
  S.RuntimeCost = clamp01(S.RuntimeCost * MixRuntimeScale);
  S.SizeCost = clamp01(S.SizeCost * MixSizeScale);
  S.CorrectnessRisk = clamp01(S.CorrectnessRisk * MixRiskScale);

  return S;
}

static PassScore estimateControlFlowComparisonScore(
    ObfPass P, const FunctionCostFeatures &F) {
  PassBaseWeight B = getBaseWeight(P);
  PassScore S{B.SecurityGain, B.DiversityGain, B.RuntimeCost, B.SizeCost,
              B.CorrectnessRisk};

  if (P == ObfPass::BCF) {
    const double Factor =
        (static_cast<double>(F.CyclomaticComplexity) - 2.0) / 4.0;
    const BCFWorkEstimate Work = estimateBCFWork(F);
    S.SecurityGain *= Factor;
    S.DiversityGain *= Factor;
    S.RuntimeCost *= Factor * Work.RuntimeMultiplier;
    S.SizeCost *= Factor * (1.0 + F.CondJumpRatio) * Work.SizeMultiplier;
    S.CorrectnessRisk *= Factor;
  } else {
    const double Factor = F.AvgCFGDepth / 3.0;
    const double ComplexityRuntimeFactor =
        1.0 + static_cast<double>(F.CyclomaticComplexity) / 6.0;
    S.SecurityGain *= Factor;
    S.DiversityGain *= Factor;
    S.RuntimeCost *= Factor * ComplexityRuntimeFactor;
    S.SizeCost *= Factor;
    S.CorrectnessRisk *= Factor;
  }

  S.SecurityGain *= MixSecurityScale;
  S.DiversityGain *= MixDiversityScale;
  S.RuntimeCost *= MixRuntimeScale;
  S.SizeCost *= MixSizeScale;
  S.CorrectnessRisk *= MixRiskScale;
  return S;
}

static PassScore estimateBasicBlockPassScore(ObfPass P,
                                             const BasicBlockCostFeatures &B) {
  PassBaseWeight Base = getBaseWeight(P);
  PassScore S{Base.SecurityGain, Base.DiversityGain, Base.RuntimeCost,
              Base.SizeCost, Base.CorrectnessRisk};

  switch (P) {
  case ObfPass::SUB:
    S.SecurityGain *= (B.SubRatio > 0) ? (1.0 + B.SubRatio) : 0.0;
    S.DiversityGain *= (B.SubRatio > 0) ? 1.0 : 0.0;
    S.RuntimeCost *= (B.SubRatio > 0) ? (1.0 + B.SubRatio) : 0.0;
    S.SizeCost *= (B.SubRatio > 0) ? (1.0 + B.SubRatio) : 0.0;
    S.CorrectnessRisk *= (B.SubRatio > 0) ? 1.0 : 0.0;
    break;
  case ObfPass::ICALL:
    S.SecurityGain *= B.HasCall ? 1.0 : 0.0;
    S.DiversityGain *= B.HasCall ? 1.0 : 0.0;
    S.RuntimeCost *= B.HasCall ? 1.0 : 0.0;
    S.SizeCost *= B.HasCall ? 1.0 : 0.0;
    S.CorrectnessRisk *= B.HasCall ? 1.0 : 0.0;
    break;
  default:
    break;
  }

  S.SecurityGain = clamp01(S.SecurityGain * MixSecurityScale);
  S.DiversityGain = clamp01(S.DiversityGain * MixDiversityScale);
  S.RuntimeCost = clamp01(S.RuntimeCost * MixRuntimeScale);
  S.SizeCost = clamp01(S.SizeCost * MixSizeScale);
  S.CorrectnessRisk = clamp01(S.CorrectnessRisk * MixRiskScale);

  return S;
}

static bool isFunctionPassApplicable(ObfPass P, const FunctionCostFeatures &F) {
  switch (P) {
  case ObfPass::IBR:
    return F.CondJumpRatio >= 0.15 && F.IBRCompatible;
  case ObfPass::BCF:
    return F.CyclomaticComplexity >= 3 &&
           F.TotalInsts <= MixBCFMaxInstructions.getValue() &&
           estimateBCFWork(F).ExpectedModifiedBB <=
               MixBCFMaxExpectedModifiedBB.getValue();
  case ObfPass::FLA:
    return F.AvgCFGDepth >= 2.0;
  case ObfPass::SPLIT:
    return F.MaxBBSize >= 5;
  case ObfPass::IGV:
    return !F.HasInlineAsm;
  case ObfPass::SUB:
  case ObfPass::ICALL:
    return false;
  }

  return false;
}

static bool isBasicBlockPassApplicable(ObfPass P,
                                       const BasicBlockCostFeatures &B) {
  switch (P) {
  case ObfPass::SUB:
    return B.SubInsts > 0;
  case ObfPass::ICALL:
    return B.HasCall && B.BBSize < 5;
  default:
    return false;
  }
}

static bool isFunctionPassNearBoundary(ObfPass P,
                                       const FunctionCostFeatures &F) {
  switch (P) {
  case ObfPass::IBR:
    return F.CondJumpRatio >= 0.15 && F.CondJumpRatio < 0.25;
  case ObfPass::BCF:
    return F.CyclomaticComplexity >= 3 && F.CyclomaticComplexity < 5;
  case ObfPass::FLA:
    return F.AvgCFGDepth >= 2.0 && F.AvgCFGDepth < 3.0;
  case ObfPass::SPLIT:
    return F.MaxBBSize >= 5 && F.MaxBBSize < 8;
  case ObfPass::IGV:
    return true;
  case ObfPass::SUB:
  case ObfPass::ICALL:
    return false;
  }

  return false;
}

static bool isBasicBlockPassNearBoundary(ObfPass P,
                                         const BasicBlockCostFeatures &B) {
  switch (P) {
  case ObfPass::SUB:
    return B.SubInsts > 0 && (B.SubInsts < 3 || B.SubRatio < 0.5);
  case ObfPass::ICALL:
    return B.HasCall && B.BBSize < 5;
  default:
    return false;
  }
}

static bool conflicts(ObfPass A, ObfPass B) {
  if ((A == ObfPass::IBR && B == ObfPass::FLA) ||
      (A == ObfPass::FLA && B == ObfPass::IBR))
    return true;

  if ((A == ObfPass::IBR && B == ObfPass::IGV) ||
      (A == ObfPass::IGV && B == ObfPass::IBR))
    return true;

  if ((A == ObfPass::SPLIT && B == ObfPass::ICALL) ||
      (A == ObfPass::ICALL && B == ObfPass::SPLIT))
    return true;

  return false;
}

/// Check whether the given pass should be skipped because of -mix-only-pass.
static bool skipDueToMixOnlyPass(ObfPass P) {
  if (MixOnlyPass.empty())
    return false;
  auto Lower = StringRef(MixOnlyPass).lower();
  bool Match = false;
  if (Lower == "sub")       Match = P == ObfPass::SUB;
  else if (Lower == "bcf")  Match = P == ObfPass::BCF;
  else if (Lower == "fla")  Match = P == ObfPass::FLA;
  else if (Lower == "split")Match = P == ObfPass::SPLIT;
  else if (Lower == "ibr")  Match = P == ObfPass::IBR;
  else if (Lower == "icall")Match = P == ObfPass::ICALL;
  else if (Lower == "igv")  Match = P == ObfPass::IGV;
  else report_fatal_error(Twine("Unknown MIXCOMPILE only-pass: ") + Lower);
  return !Match;
}

static std::vector<ObfPass> selectFunctionPasses(const FunctionCostFeatures &F,
                                                 const CostModelWeights &W,
                                                 double Threshold,
                                                 StringRef Mode) {
  std::vector<std::pair<ObfPass, double>> Candidates;

  for (ObfPass P : {ObfPass::IBR, ObfPass::BCF, ObfPass::FLA, ObfPass::SPLIT,
                    ObfPass::IGV}) {
    if (skipDueToMixOnlyPass(P))
      continue;
    if (!isFunctionPassApplicable(P, F))
      continue;

    PassScore Score = estimateFunctionPassScore(P, F);
    double Total = Score.total(W);

    bool Enable = false;
    if (Mode == "all")
      Enable = true;
    else if (Mode == "random")
      Enable = choose_machine();
    else if (Mode == "rule")
      Enable = !isFunctionPassNearBoundary(P, F) || choose_machine();
    else if (Total > Threshold)
      Enable = !isFunctionPassNearBoundary(P, F) || choose_machine();

    if (Enable)
      Candidates.push_back({P, Total});
  }

  auto FindCandidate = [&Candidates](ObfPass P) {
    return std::find_if(Candidates.begin(), Candidates.end(),
                        [P](const auto &Candidate) {
                          return Candidate.first == P;
                        });
  };
  auto IBRIt = FindCandidate(ObfPass::IBR);
  auto BCFIt = FindCandidate(ObfPass::BCF);
  auto FLAIt = FindCandidate(ObfPass::FLA);
  if (IBRIt != Candidates.end() && FLAIt != Candidates.end()) {
    Candidates.erase(FLAIt);
  } else if (BCFIt != Candidates.end() && FLAIt != Candidates.end()) {
    const PassScore BCFCompare =
        estimateControlFlowComparisonScore(ObfPass::BCF, F);
    const PassScore FLACompare =
        estimateControlFlowComparisonScore(ObfPass::FLA, F);
    const double BCFCompareTotal = BCFCompare.total(W);
    const double FLACompareTotal = FLACompare.total(W);
    const bool SelectBCF =
        BCFCompareTotal > FLACompareTotal ||
        (BCFCompareTotal == FLACompareTotal && BCFIt->second >= FLAIt->second);
    Candidates.erase(SelectBCF ? FLAIt : BCFIt);
  }

  std::sort(Candidates.begin(), Candidates.end(),
            [](const auto &A, const auto &B) {
              if (A.first == ObfPass::IBR && B.first != ObfPass::IBR)
                return true;
              if (A.first != ObfPass::IBR && B.first == ObfPass::IBR)
                return false;
              return A.second > B.second;
            });

  std::vector<ObfPass> Selected;
  for (const auto &Candidate : Candidates) {
    ObfPass P = Candidate.first;
    bool HasConflict = false;

    for (ObfPass Existing : Selected) {
      if (conflicts(P, Existing)) {
        HasConflict = true;
        break;
      }
    }

    if (!HasConflict)
      Selected.push_back(P);
  }

  return Selected;
}

static bool shouldEnableBasicBlockPass(ObfPass P,
                                       const BasicBlockCostFeatures &B,
                                       const CostModelWeights &W,
                                       double Threshold,
                                       StringRef Mode) {
  if (!isBasicBlockPassApplicable(P, B))
    return false;

  PassScore Score = estimateBasicBlockPassScore(P, B);
  double Total = Score.total(W);

  if (Mode == "all")
    return true;
  if (Mode == "random")
    return choose_machine();
  if (Mode == "rule")
    return !isBasicBlockPassNearBoundary(P, B) || choose_machine();

  if (Total <= Threshold)
    return false;

  if (isBasicBlockPassNearBoundary(P, B))
    return choose_machine();

  return true;
}

static bool containsPass(const std::vector<ObfPass> &Passes, ObfPass P) {
  return std::find(Passes.begin(), Passes.end(), P) != Passes.end();
}

static void annotateFunctionPasses(Function &Fn,
                                   const std::vector<ObfPass> &Selected) {
  add_annotation(Fn, containsPass(Selected, ObfPass::IBR) ? "ibr" : "noibr",
                 "IBR_annotations");
  add_annotation(Fn, containsPass(Selected, ObfPass::BCF) ? "bcf" : "nobcf",
                 "BCF_annotations");
  add_annotation(Fn, containsPass(Selected, ObfPass::FLA) ? "fla" : "nofla",
                 "FLA_annotations");
  add_annotation(Fn,
                 containsPass(Selected, ObfPass::SPLIT) ? "split" : "nosplit",
                 "SPLIT_annotations");
  add_annotation(Fn, containsPass(Selected, ObfPass::IGV) ? "igv" : "noigv",
                 "IGV_annotations");
}
} // namespace
void add_annotation(Function &Fn, const char *annotation,
                    const char *annotation_name) {
  LLVMContext &Ctx = Fn.getContext();
  MDString *AnnotStr = MDString::get(Ctx, annotation);
  MDNode *AnnotMD = MDNode::get(Ctx, AnnotStr);
  Fn.setMetadata(annotation_name, AnnotMD);
  errs() << "Successfully set " << annotation << " annotation for function "
         << Fn.getName() << ": " << annotation << "\n";
}

void add_annotation_bb(BasicBlock &BB, const char *annotation,
                       const char *annotation_name) {
  LLVMContext &Ctx = BB.getContext();
  Instruction &firstInst = *BB.begin();
  MDString *AnnotStr = MDString::get(Ctx, annotation);
  MDNode *AnnotMD = MDNode::get(Ctx, AnnotStr);
  firstInst.setMetadata(annotation_name, AnnotMD);
  errs() << "Successfully set " << annotation
         << " annotation for the basicblock " << BB.getName() << "in function "
         << BB.getParent()->getName() << ": " << annotation << "\n";
}

double calculateFuncBBAverageDepth(Function &F) {
  if (F.empty())
    return 0.0;

  std::map<BasicBlock *, unsigned> BBDepth;
  std::queue<BasicBlock *> BBQueue;

  BasicBlock *EntryBB = &F.front();
  BBDepth[EntryBB] = 1;
  BBQueue.push(EntryBB);

  while (!BBQueue.empty()) {
    BasicBlock *CurrentBB = BBQueue.front();
    BBQueue.pop();
    unsigned CurrentDepth = BBDepth[CurrentBB];

    for (succ_iterator It = succ_begin(CurrentBB), End = succ_end(CurrentBB);
         It != End; ++It) {
      BasicBlock *SuccBB = *It;
      if (BBDepth.find(SuccBB) == BBDepth.end()) {
        BBDepth[SuccBB] = CurrentDepth + 1;
        BBQueue.push(SuccBB);
      }
    }
  }

    unsigned TotalDepth = 0;
  for (auto &Pair : BBDepth) {
    dbgs() << "Depth is " << Pair.second << "\n";
    TotalDepth += Pair.second;
  }

  return (double)TotalDepth / BBDepth.size();
}


bool choose_machine() {
  static std::mt19937_64 gen(getMixEffectiveSeed());
  static std::uniform_int_distribution<> dist(1, 100);
  int pro_num = dist(gen);
  while (pro_num == 50)
    pro_num = dist(gen);
  return pro_num < 50;
}

bool checkBasicBlockPreds(const BasicBlock *BB,
                          const BasicBlock *TargetPredBB) {
  if (!BB || !TargetPredBB)
    return false;
  std::set<const BasicBlock *> Visited;
  while (BB != TargetPredBB) {
    if (!Visited.insert(BB).second)
      return false;
    const Instruction *Term = BB->getTerminator();
    if (const BranchInst *BI = dyn_cast<BranchInst>(Term)) {
      if (BI->isConditional()) {
        return false;
      }
    }
    if (pred_size(BB) != 1) {
      return false;
    }
    BB = *predecessors(BB).begin();
  }
  return true;
}

PreservedAnalyses PassDecider::run(Module &M, ModuleAnalysisManager &AM) {
  if (this->flag)
    outs() << "[Soule] force.run.PassDecider\n";

  validateCommandLineOptions();
  initializeMixRandomSeed(getMixRequestedSeed());
  loadCostConfig();
  CostModelWeights Weights = getProfileWeights(MixProfile.getValue());
  double ScoreThreshold = MixScoreThreshold.getValue();
  StringRef Mode = MixMode.getValue();

  for (Function &Fn : M) {
    if (!toObfuscate(flag, &Fn, "pd"))
      continue;

    if (Fn.empty() || Fn.hasLinkOnceLinkage() ||
        Fn.getSection() == ".text.startup")
      continue;

    SplitAllCriticalEdges(Fn, CriticalEdgeSplittingOptions(nullptr, nullptr));
    dbgs() << "Function name is " << Fn.getName() << "\n";

    bool IBRCompatible = true;
    FunctionCostFeatures CostFeatures;
    std::vector<std::pair<BasicBlock *, BasicBlockCostFeatures>> BBCostFeatures;

    for (BasicBlock &BB : Fn) {
      BasicBlockCostFeatures BBCost;
      CostFeatures.NumBBs++;
      CostFeatures.NumEdges += succ_size(&BB);

      const Instruction *Term = BB.getTerminator();
      if (const BranchInst *BI = dyn_cast<BranchInst>(Term)) {
        CostFeatures.NumTotalJumps++;
        if (BI->isConditional())
          CostFeatures.NumCondJumps++;
      } else if (isa<SwitchInst>(Term)) {
        CostFeatures.NumTotalJumps++;
        CostFeatures.NumCondJumps++;
      } else if (isa<IndirectBrInst>(Term)) {
        CostFeatures.NumTotalJumps++;
        CostFeatures.NumCondJumps++;
      }

      for (Instruction &DefInst : BB) {
        BBCost.BBSize++;

        if (isSubstitutableOpcode(DefInst.getOpcode()))
          BBCost.SubInsts++;

        if (auto *CI = dyn_cast<CallBase>(&DefInst)) {
          BBCost.HasCall = true;
          if (isa<InlineAsm>(CI->getCalledOperand()))
            BBCost.HasInlineAsm = true;
        }

        if (DefInst.use_empty() || !IBRCompatible)
          continue;

        dbgs() << "Instruction is " << DefInst << "\n";
        for (Use &U : DefInst.uses()) {
          User *UseUser = U.getUser();

          if (PHINode *Phi = dyn_cast<PHINode>(UseUser)) {
            if (Phi->getParent() == &BB) {
              IBRCompatible = false;
              break;
            }
          } else if (Instruction *UseInst = dyn_cast<Instruction>(UseUser)) {
            const BasicBlock *UseBB = UseInst->getParent();
            if (&BB != UseBB) {
              if (pred_size(UseBB) != 1) {
                IBRCompatible = false;
                break;
              }

              const BasicBlock *PredBB = *predecessors(UseBB).begin();
              if (!checkBasicBlockPreds(PredBB, &BB)) {
                IBRCompatible = false;
                break;
              }
            }
          }
        }
      }

      BBCost.SubRatio = safeRatio(BBCost.SubInsts, BBCost.BBSize);
      CostFeatures.TotalInsts += BBCost.BBSize;
      CostFeatures.TotalSubInsts += BBCost.SubInsts;
      CostFeatures.MaxBBSize = std::max(CostFeatures.MaxBBSize, BBCost.BBSize);
      CostFeatures.HasCall |= BBCost.HasCall;
      CostFeatures.HasInlineAsm |= BBCost.HasInlineAsm;
      BBCostFeatures.push_back({&BB, BBCost});
    }

    CostFeatures.CyclomaticComplexity =
        CostFeatures.NumEdges >= CostFeatures.NumBBs
            ? CostFeatures.NumEdges - CostFeatures.NumBBs + 2
            : 1;
    CostFeatures.CondJumpRatio =
        safeRatio(CostFeatures.NumCondJumps, CostFeatures.NumTotalJumps);
    CostFeatures.AvgCFGDepth = calculateFuncBBAverageDepth(Fn);
    CostFeatures.SubRatio =
        safeRatio(CostFeatures.TotalSubInsts, CostFeatures.TotalInsts);
    CostFeatures.IBRCompatible = IBRCompatible;

    dbgs() << "average cfg depth is" << CostFeatures.AvgCFGDepth << "\n";
    errs() << "[CFG] Function: " << Fn.getName() << "\n";
    errs() << "  Cyclomatic Complexity: " << CostFeatures.CyclomaticComplexity
           << " (E=" << CostFeatures.NumEdges << ", N=" << CostFeatures.NumBBs
           << ")\n";
    errs() << "  Conditional Jump Ratio: " << CostFeatures.CondJumpRatio << " ("
           << CostFeatures.NumCondJumps << "/" << CostFeatures.NumTotalJumps
           << ")\n";

    std::vector<ObfPass> SelectedFunctionPasses =
        selectFunctionPasses(CostFeatures, Weights, ScoreThreshold, Mode);
    const BCFWorkEstimate BCFWork = estimateBCFWork(CostFeatures);
    const bool BCFBudgetEligible =
        CostFeatures.TotalInsts <= MixBCFMaxInstructions.getValue() &&
        BCFWork.ExpectedModifiedBB <=
            MixBCFMaxExpectedModifiedBB.getValue();
    const double BCFFirstTotal =
        estimateFunctionPassScore(ObfPass::BCF, CostFeatures).total(Weights);
    const double FLAFirstTotal =
        estimateFunctionPassScore(ObfPass::FLA, CostFeatures).total(Weights);
    const double BCFCompareTotal =
        estimateControlFlowComparisonScore(ObfPass::BCF, CostFeatures)
            .total(Weights);
    const double FLACompareTotal =
        estimateControlFlowComparisonScore(ObfPass::FLA, CostFeatures)
            .total(Weights);
    errs() << "[MIXCOMPILE][CF_DECISION]"
           << " function=" << Fn.getName()
           << " NumBBs=" << CostFeatures.NumBBs
           << " TotalInsts=" << CostFeatures.TotalInsts
           << " CyclomaticComplexity=" << CostFeatures.CyclomaticComplexity
           << " CondJumpRatio=" << CostFeatures.CondJumpRatio
           << " AvgCFGDepth=" << CostFeatures.AvgCFGDepth
           << " bcf_prob=" << BCFWork.Probability
           << " bcf_loop=" << BCFWork.LoopCount
           << " ExpectedModifiedBB=" << BCFWork.ExpectedModifiedBB
           << " BCFMaxInstructions=" << MixBCFMaxInstructions.getValue()
           << " BCFMaxExpectedModifiedBB="
           << MixBCFMaxExpectedModifiedBB.getValue()
           << " BCFBudgetEligible=" << (BCFBudgetEligible ? 1 : 0)
           << " RuntimeMultiplier=" << BCFWork.RuntimeMultiplier
           << " SizeMultiplier=" << BCFWork.SizeMultiplier
           << " BCF_first_total=" << BCFFirstTotal
           << " FLA_first_total=" << FLAFirstTotal
           << " BCF_compare_total=" << BCFCompareTotal
           << " FLA_compare_total=" << FLACompareTotal
           << " selected_pass=";
    bool FirstSelectedPass = true;
    for (ObfPass P : SelectedFunctionPasses) {
      if (!FirstSelectedPass)
        errs() << ',';
      errs() << passToString(P);
      FirstSelectedPass = false;
    }
    if (FirstSelectedPass)
      errs() << "NONE";
    errs() << "\n";
    annotateFunctionPasses(Fn, SelectedFunctionPasses);

    bool FunctionHasSplit =
        containsPass(SelectedFunctionPasses, ObfPass::SPLIT);

    for (auto &Item : BBCostFeatures) {
      BasicBlock *CostBB = Item.first;
      const BasicBlockCostFeatures &BBCost = Item.second;

      if (!skipDueToMixOnlyPass(ObfPass::SUB) &&
          shouldEnableBasicBlockPass(ObfPass::SUB, BBCost, Weights,
                                     ScoreThreshold, Mode))
        add_annotation_bb(*CostBB, "sub", "SUB_annotations");

      if (!skipDueToMixOnlyPass(ObfPass::ICALL) &&
          !FunctionHasSplit &&
          shouldEnableBasicBlockPass(ObfPass::ICALL, BBCost, Weights,
                                     ScoreThreshold, Mode))
        add_annotation_bb(*CostBB, "icall", "ICALL_annotations");
    }
  }

  dumpCostConfig();
  return PreservedAnalyses::none();
}
PassDecider *llvm::createPassDecider(bool flag) {
  return new PassDecider(flag);
}
