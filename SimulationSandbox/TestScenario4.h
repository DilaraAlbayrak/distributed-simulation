#pragma once
#include "Scenario.h"

class TestScenario4 : public Scenario
{
protected:
	void setupFixedObjects() override;

public:
	TestScenario4(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : Scenario(pDevice, pContext) {}
	~TestScenario4() = default;

	void onLoad() override;
	void onUnload() override;
	void onUpdate(float dt = 0.016f) override;
	void ImGuiMainMenu() override;
};