/*
Syn's AyyWare Framework 2015
*/

#pragma once
#include "SDK.h"

void InitKeyvalues();

void ForceMaterial(Color color, IMaterial* material, bool useColor = true, bool forceMaterial = true);

IMaterial *CreateMaterial(bool shouldIgnoreZ, bool isLit = true, bool isWireframe = false);