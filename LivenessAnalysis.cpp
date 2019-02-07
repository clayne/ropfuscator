//
// LivenessAnalysis
// Analyses the liveness of physical registers in order to get an unused
// (dead/killed) register when we have the need of a scratch register
//

#include "LivenessAnalysis.h"
#include "../X86.h"
#include "../X86Subtarget.h"
#include "CapstoneLLVMAdpt.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <capstone/capstone.h>
#include <capstone/x86.h>
#include <map>

using namespace llvm;

ScratchRegTracker::ScratchRegTracker(MachineBasicBlock &MBB) : MBB(MBB) {
  performLivenessAnalysis();
}

void ScratchRegTracker::addReg(MachineInstr &MI, int reg) {
  auto it = regs.find(&MI);
  // IMPORTANT: Here the register representation is converted from LLVM to
  // capstone and stored in the map.
  if (it != regs.end()) {
    it->second.push_back(convertToCapstoneReg(reg));
  } else {
    std::vector<x86_reg> tmp;
    tmp.push_back(convertToCapstoneReg(reg));
    regs.insert(std::make_pair(&MI, tmp));
  }
}

std::vector<x86_reg> *ScratchRegTracker::findRegs(MachineInstr &MI) {
  auto it = regs.find(&MI);
  if (it != regs.end()) {
    std::vector<x86_reg> *tmp = &it->second;
    if (tmp->size() > 0) {
      return tmp;
    }
  }
  return nullptr;
}

x86_reg ScratchRegTracker::getReg(MachineInstr &MI) {
  std::vector<x86_reg> *tmp = findRegs(MI);
  if (tmp)
    return tmp->back();
  return X86_REG_INVALID;
}

std::vector<x86_reg> *ScratchRegTracker::getRegs(MachineInstr &MI) {
  std::vector<x86_reg> *tmp = findRegs(MI);
  if (tmp)
    return tmp;
  return nullptr;
}

/*int ScratchRegTracker::popReg(MachineInstr &MI) {
  std::vector<int> *tmp = findRegs(MI);
  if (tmp) {
    int retval = tmp->back();
    tmp->pop_back();
    return retval;
  }

  return NULL;
}

int ScratchRegTracker::popReg(MachineInstr &MI, int reg) {
  std::vector<int> *tmp = findRegs(MI);
  auto it = find(tmp->begin(), tmp->end(), reg);
  if (it != tmp->end()) {
    int retVal = *it;
    tmp->erase(it);
    return retVal;
  }

  // assert(false && "Unable to pop given register! Not found!");
  return NULL;
}
*/
int ScratchRegTracker::count(MachineInstr &MI) {
  std::vector<x86_reg> *tmp = findRegs(MI);
  if (tmp)
    return tmp->size();
  return 0;
}

void ScratchRegTracker::performLivenessAnalysis() {
  const MachineFunction *MF = MBB.getParent();
  const TargetRegisterInfo &TRI = *MF->getSubtarget().getRegisterInfo();
  const MachineRegisterInfo &MRI = MF->getRegInfo();
  LivePhysRegs LiveRegs(TRI);
  LiveRegs.addLiveIns(MBB);

  // Data-flow analysis is performed starting from the end of each basic block,
  // iterating each instruction backwards to find USEs and DEFs of each physical
  // register
  for (auto I = MBB.begin(); I != MBB.end(); ++I) {
    MachineInstr *MI = &*I;

    for (unsigned reg : X86::GR32RegClass) {
      if (LiveRegs.available(MRI, reg)) {
        addReg(*MI, reg);
      }
    }
    SmallVector<std::pair<unsigned, const MachineOperand *>, 2> Clobbers;
    LiveRegs.stepForward(*MI, Clobbers);
  }

  dbgs() << "[*] Register liveness analysis performed on basic block "
         << MBB.getNumber() << "\n";
}