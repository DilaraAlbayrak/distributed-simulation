#pragma once
#include "Scenario.h"

class Scenario3 :  public Scenario
{
protected:
	void setupFixedObjects() override;

public:
	Scenario3(const CComPtr <ID3D11Device>& pDevice, const CComPtr <ID3D11DeviceContext>& pContext) : Scenario(pDevice, pContext)
	{
		setNumMovingSpheres(10000);
		setMinRadius(0.01f);
		setMaxRadius(0.03f);
	}
	~Scenario3() = default;

	void onLoad() override;
	void onUnload() override;
	void onUpdate(float dt = 0.016f) override;
	void ImGuiMainMenu() override;
};

