#include "stdafx.h"
#include "CommandContext.h"

void ContextManager::DestroyAllContexts()
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		sm_contextPool[i].clear();
	}
}

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	std::lock_guard<std::mutex> LockGuard(sm_contextAllocationMutex);

	auto& AvailableContexts = sm_availableContexts[Type];

	CommandContext* ret = nullptr;
	if (AvailableContexts.empty())
	{
		ret = new CommandContext(Type);
		sm_contextPool[Type].emplace_back(ret);
		ret->Initialize();
	}
	else
	{
		ret = AvailableContexts.front();
		AvailableContexts.pop();
		ret->Reset();
	}
	assert(ret != nullptr);

	assert(ret->m_type == Type);

	return ret;
}

void ContextManager::FreeContext(CommandContext* usedContext)
{
	assert(usedContext != nullptr);
	std::lock_guard<std::mutex> LockGuard(sm_contextAllocationMutex);
	sm_availableContexts[usedContext->m_type].push(usedContext);
}

CommandContext& CommandContext::Begin(const std::wstring& ID /* = L"" */)
{
	CommandContext* newContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	newContext->SetID(ID);
	return *newContext;
}