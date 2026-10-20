#include "sim/math/mat4.hpp"

#include <cmath>

namespace Sim
{
	namespace
	{
		float ToRadians(float degrees) { return degrees * 3.14159265358979f / 180.0f; }

		float& At(Mat4& m, int row, int col) { return m.Values[col * 4 + row]; }
		float  At(Mat4 const& m, int row, int col) { return m.Values[col * 4 + row]; }
	}

	Mat4 Multiply(Mat4 const& a, Mat4 const& b)
	{
		Mat4 result;
		for (int row = 0; row < 4; ++row)
		{
			for (int col = 0; col < 4; ++col)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
				{
					sum += At(a, row, k) * At(b, k, col);
				}
				At(result, row, col) = sum;
			}
		}
		return result;
	}

	Mat4 Translate(Vec3 offset)
	{
		Mat4 m;
		At(m, 0, 3) = offset.x;
		At(m, 1, 3) = offset.y;
		At(m, 2, 3) = offset.z;
		return m;
	}

	Mat4 Scale(Vec3 factors)
	{
		Mat4 m;
		At(m, 0, 0) = factors.x;
		At(m, 1, 1) = factors.y;
		At(m, 2, 2) = factors.z;
		return m;
	}

	Mat4 RotateXDeg(float degrees)
	{
		Mat4        m;
		float const c = std::cos(ToRadians(degrees));
		float const s = std::sin(ToRadians(degrees));
		At(m, 1, 1)   = c;
		At(m, 1, 2)   = -s;
		At(m, 2, 1)   = s;
		At(m, 2, 2)   = c;
		return m;
	}

	Mat4 RotateYDeg(float degrees)
	{
		Mat4        m;
		float const c = std::cos(ToRadians(degrees));
		float const s = std::sin(ToRadians(degrees));
		At(m, 0, 0)   = c;
		At(m, 0, 2)   = s;
		At(m, 2, 0)   = -s;
		At(m, 2, 2)   = c;
		return m;
	}

	Mat4 RotateZDeg(float degrees)
	{
		Mat4        m;
		float const c = std::cos(ToRadians(degrees));
		float const s = std::sin(ToRadians(degrees));
		At(m, 0, 0)   = c;
		At(m, 0, 1)   = -s;
		At(m, 1, 0)   = s;
		At(m, 1, 1)   = c;
		return m;
	}

	Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up)
	{
		Vec3 const forward = Normalize(target - eye);
		Vec3 const right   = Normalize(Cross(forward, up));
		Vec3 const trueUp  = Cross(right, forward);

		Mat4 m;
		At(m, 0, 0) = right.x;
		At(m, 0, 1) = right.y;
		At(m, 0, 2) = right.z;
		At(m, 1, 0) = trueUp.x;
		At(m, 1, 1) = trueUp.y;
		At(m, 1, 2) = trueUp.z;
		At(m, 2, 0) = -forward.x;
		At(m, 2, 1) = -forward.y;
		At(m, 2, 2) = -forward.z;
		At(m, 0, 3) = -Dot(right, eye);
		At(m, 1, 3) = -Dot(trueUp, eye);
		At(m, 2, 3) = Dot(forward, eye);
		return m;
	}

	Mat4 Perspective(float fovYDeg, float aspect, float nearPlane, float farPlane)
	{
		float const f = 1.0f / std::tan(ToRadians(fovYDeg) * 0.5f);
		Mat4        m;
		At(m, 0, 0) = f / aspect;
		At(m, 1, 1) = f;
		At(m, 2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
		At(m, 2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
		At(m, 3, 2) = -1.0f;
		At(m, 3, 3) = 0.0f;
		return m;
	}

	Mat4 Inverse(Mat4 const& m)
	{
		// Gauss-Jordan elimination with partial pivoting on [m | I].
		float a[4][8];
		for (int row = 0; row < 4; ++row)
		{
			for (int col = 0; col < 4; ++col)
			{
				a[row][col]     = At(m, row, col);
				a[row][col + 4] = (row == col) ? 1.0f : 0.0f;
			}
		}
		for (int col = 0; col < 4; ++col)
		{
			int pivot = col;
			for (int row = col + 1; row < 4; ++row)
			{
				if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
				{
					pivot = row;
				}
			}
			if (std::fabs(a[pivot][col]) < 1e-12f)
			{
				return Mat4{};
			}
			if (pivot != col)
			{
				for (int k = 0; k < 8; ++k)
				{
					float const tmp = a[col][k];
					a[col][k]       = a[pivot][k];
					a[pivot][k]     = tmp;
				}
			}
			float const diag = a[col][col];
			for (int k = 0; k < 8; ++k)
			{
				a[col][k] /= diag;
			}
			for (int row = 0; row < 4; ++row)
			{
				if (row == col)
				{
					continue;
				}
				float const factor = a[row][col];
				for (int k = 0; k < 8; ++k)
				{
					a[row][k] -= factor * a[col][k];
				}
			}
		}
		Mat4 result;
		for (int row = 0; row < 4; ++row)
		{
			for (int col = 0; col < 4; ++col)
			{
				At(result, row, col) = a[row][col + 4];
			}
		}
		return result;
	}

	Vec3 TransformPoint(Mat4 const& m, Vec3 p)
	{
		float const x = At(m, 0, 0) * p.x + At(m, 0, 1) * p.y + At(m, 0, 2) * p.z + At(m, 0, 3);
		float const y = At(m, 1, 0) * p.x + At(m, 1, 1) * p.y + At(m, 1, 2) * p.z + At(m, 1, 3);
		float const z = At(m, 2, 0) * p.x + At(m, 2, 1) * p.y + At(m, 2, 2) * p.z + At(m, 2, 3);
		float const w = At(m, 3, 0) * p.x + At(m, 3, 1) * p.y + At(m, 3, 2) * p.z + At(m, 3, 3);
		return (std::fabs(w) > 1e-12f) ? Vec3{x / w, y / w, z / w} : Vec3{x, y, z};
	}

	Vec3 TransformDirection(Mat4 const& m, Vec3 d)
	{
		return {At(m, 0, 0) * d.x + At(m, 0, 1) * d.y + At(m, 0, 2) * d.z,
		        At(m, 1, 0) * d.x + At(m, 1, 1) * d.y + At(m, 1, 2) * d.z,
		        At(m, 2, 0) * d.x + At(m, 2, 1) * d.y + At(m, 2, 2) * d.z};
	}
}
