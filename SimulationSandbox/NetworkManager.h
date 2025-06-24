#pragma once
#include <memory>  

class NetworkManager
{
private:
	static std::unique_ptr<NetworkManager> _instance;

public:
	static NetworkManager& getInstance()
	{
		if (!_instance)
			_instance = std::make_unique<NetworkManager>();
		return *_instance;
	}
};

