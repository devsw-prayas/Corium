#include "Corium.h"
#include "CoriumMemoryHandler.h"
#include "Inductor.h"
#include "PipelineUtils.h"
#include "CoriumEnvironment.h"
#include "CoriumRuntime.h"

#include <cstring>

namespace Corium::Execution::Inductor {

    bool TaskInductor::init(TaskDesc& ro_Desc, uint32_t v_Node) {
        if (!CoriumRuntime::isRuntimeInit()) return false;
        if (v_Node >= Environment::EnvironmentProbe::getCpuInfo().m_NumaNodeCount) return false;

        auto& allocators = Memory::Internal::AtomicAllocators::instance();
        ro_Desc.m_MemDesc = Memory::SharedPtr<TaskMemoryDesc, Memory::Allocators::TaskMetadataAllocator>::make(
            allocators.s_TaskMemoryDescAllocator[v_Node].load(),
            allocators.s_ControlBlockAllocator[v_Node].load()
        );
        if (!ro_Desc.m_MemDesc) return false;

        ro_Desc.m_NumaNode = v_Node;
        ro_Desc.m_State    = TaskDescState::MUTABLE;
        return true;
    }

    bool TaskInductor::validate(const TaskDesc& ro_Desc) {
        if (ro_Desc.m_State != TaskDescState::MUTABLE) return false;
        if (!ro_Desc.m_MemDesc) return false;
        if (ro_Desc.m_MemDesc->m_MemState != ParamMemState::FROZEN) return false;
        if (!ro_Desc.m_MemDesc->m_InputMPH.isValid()) return false;
        if (!ro_Desc.m_MemDesc->m_OutputMPH.isValid()) return false;
        return true;
    }

    TaskFrame TaskInductor::cook(TaskDesc& ro_Desc) {
        if (!validate(ro_Desc)) return TaskFrame{};

        const uint32_t node   = ro_Desc.m_NumaNode;
        const uint32_t inputSize   = ro_Desc.m_MemDesc->m_InputBufferSize;
        const uint32_t outputSize  = ro_Desc.m_MemDesc->m_OutputBufferSize;

        auto& allocators   = Memory::Internal::AtomicAllocators::instance();
        auto* payloadAllocator = allocators.s_TaskPayloadAllocator[node].load();

        void* input  = payloadAllocator->allocateImpl(inputSize,  32);
        void* output = payloadAllocator->allocateImpl(outputSize, 32);
        if (!input || !output) return TaskFrame{};

        std::memset(output, 0, sizeof(Corium::Execution::TaskError));

        ro_Desc.m_State = TaskDescState::FROZEN;

        Memory::WeakPtr<TaskMemoryDesc, Memory::Allocators::TaskMetadataAllocator> weakMemoryDesc{ ro_Desc.m_MemDesc };

        Memory::Internal::VARegion inputBuffer { static_cast<uint8_t*>(input),  inputSize  };
        Memory::Internal::VARegion outputBuffer{ static_cast<uint8_t*>(output), outputSize };

        return TaskFrame{ std::move(weakMemoryDesc), inputBuffer, outputBuffer };
    }

    bool TaskInductor::reset(TaskDesc& ro_Desc) {
        ro_Desc.m_MemDesc.reset();
        ro_Desc.m_State    = TaskDescState::UNINITIALIZED;
        ro_Desc.m_NumaNode = 0;
        return true;
    }
}
