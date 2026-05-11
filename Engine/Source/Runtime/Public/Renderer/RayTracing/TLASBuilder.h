/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TLASBuilder - Top Level Acceleration Structure Builder
 *
 * Builds TLAS from BLAS instances with optional transform matrices.
 */

#pragma once

#include "Math/MathGLM.h"
#include <nvrhi/nvrhi.h>

#include <memory>
#include <vector>

class FTLASBuilder
{
public:
	/**
	 * @brief Instance descriptor for TLAS construction
	 */
	struct FInstanceDesc
	{
		nvrhi::rt::AccelStructHandle BottomLevelAS;           //!< BLAS to instantiate
		float					Transform[12];             //!< Row-major 3x4 transform matrix
		std::uint32_t			InstanceMask{ 1 };         //!< Ray visibility mask
		nvrhi::rt::InstanceFlags InstanceFlags{ nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise };

		FInstanceDesc()
		{
			// Default identity transform
			Transform[0]  = 1.0f; Transform[1]  = 0.0f; Transform[2]  = 0.0f; Transform[3]  = 0.0f;
			Transform[4]  = 0.0f; Transform[5]  = 1.0f; Transform[6]  = 0.0f; Transform[7]  = 0.0f;
			Transform[8]  = 0.0f; Transform[9]  = 0.0f; Transform[10] = 1.0f; Transform[11] = 0.0f;
		}

		explicit FInstanceDesc(const FMat4& InMatrix)
		{
			SetTransform(InMatrix);
			InstanceMask  = 1;
			InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
		}

		/**
		 * @brief Set transform from 4x4 matrix (translation column is ignored for 3x4 transform)
		 */
		void SetTransform(const FMat4& InMatrix)
		{
			// Extract 3x4 transform (row-major) from 4x4 matrix
			// Row 0: m[0][0], m[0][1], m[0][2], m[0][3]
			// Row 1: m[1][0], m[1][1], m[1][2], m[1][3]
			// Row 2: m[2][0], m[2][1], m[2][2], m[2][3]
			Transform[0]  = InMatrix[0][0]; Transform[1]  = InMatrix[0][1]; Transform[2]  = InMatrix[0][2]; Transform[3]  = InMatrix[0][3];
			Transform[4]  = InMatrix[1][0]; Transform[5]  = InMatrix[1][1]; Transform[6]  = InMatrix[1][2]; Transform[7]  = InMatrix[1][3];
			Transform[8]  = InMatrix[2][0]; Transform[9]  = InMatrix[2][1]; Transform[10] = InMatrix[2][2]; Transform[11] = InMatrix[2][3];
			InstanceMask  = 1;
			InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
		}

		/**
		 * @brief Set transform as translation offset from identity
		 */
		void SetTranslation(const FVec3& Translation)
		{
			Transform[0]  = 1.0f; Transform[1]  = 0.0f; Transform[2]  = 0.0f; Transform[3]  = Translation.x;
			Transform[4]  = 0.0f; Transform[5]  = 1.0f; Transform[6]  = 0.0f; Transform[7]  = Translation.y;
			Transform[8]  = 0.0f; Transform[9]  = 0.0f; Transform[10] = 1.0f; Transform[11] = Translation.z;
			InstanceMask  = 1;
			InstanceFlags = nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
		}
	};

	FTLASBuilder()          = default;
	virtual ~FTLASBuilder() = default;

	FTLASBuilder(const FTLASBuilder&) = delete;
	FTLASBuilder& operator=(const FTLASBuilder&) = delete;
	FTLASBuilder(FTLASBuilder&&)                 = delete;
	FTLASBuilder& operator=(FTLASBuilder&&) = delete;

	/**
	 * @brief Initialize TLAS builder with device and max instances
	 * @param Device NVRHI device handle
	 * @param MaxInstances Maximum number of instances the TLAS can hold
	 * @return true if initialization succeeded
	 */
	[[nodiscard]] bool Initialize(nvrhi::IDevice* Device, std::uint32_t MaxInstances);

	/**
	 * @brief Add a BLAS instance to the TLAS
	 * @param Instance The instance descriptor
	 * @return true if added successfully
	 */
	[[nodiscard]] bool AddInstance(const FInstanceDesc& Instance);

	/**
	 * @brief Build the TLAS using the provided command list
	 * @param CommandList Command list to record TLAS build commands
	 * @return true if build succeeded
	 */
	[[nodiscard]] bool Build(nvrhi::ICommandList* CommandList);

	/**
	 * @brief Get the built TLAS acceleration structure
	 * @return TLAS handle
	 */
	[[nodiscard]] nvrhi::rt::AccelStructHandle GetTLAS() const { return TLas; }

	/**
	 * @brief Get raw pointer to TLAS for binding
	 * @return TLAS raw pointer
	 */
	[[nodiscard]] nvrhi::rt::IAccelStruct* GetTLASPtr() const { return TLas.Get(); }

	/**
	 * @brief Get number of instances currently in the TLAS
	 * @return Instance count
	 */
	[[nodiscard]] std::uint32_t GetInstanceCount() const { return static_cast<std::uint32_t>(Instances.size()); }

	/**
	 * @brief Check if TLAS has been built
	 * @return true if TLAS exists
	 */
	[[nodiscard]] bool IsBuilt() const { return TLas != nullptr; }

	/**
	 * @brief Clear all instances and reset the builder
	 */
	void Reset();

private:
	nvrhi::IDevice*										Device{ nullptr };
	nvrhi::rt::AccelStructHandle						TLas;
	std::vector<FInstanceDesc>							Instances;
	std::uint32_t										MaxInstances{ 0 };
	bool												bIsInitialized{ false };
};
