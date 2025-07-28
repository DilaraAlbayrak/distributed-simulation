#pragma once
#include "Scenario.h"

class Scenario5 : public Scenario
{
protected:
	void setupFixedObjects() override;

public:
	Scenario5(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : Scenario(pDevice, pContext)
	{
		setNumMovingSpheres(100);
		setMinRadius(0.1f);
		setMaxRadius(0.2f);
	}
	~Scenario5() = default;

	void onLoad() override;
	void onUnload() override;
	void onUpdate(float dt = 0.016f) override;
	void ImGuiMainMenu() override;
};