#pragma once

#include "Component/Component.h"

#include <string>

struct ModelComponent : public Component
{
	// The GameObject keeps only the key of the shared model resource.
	// Mesh, bone, and animation data are owned by the resource layer.
	std::string resourceKey;
};
