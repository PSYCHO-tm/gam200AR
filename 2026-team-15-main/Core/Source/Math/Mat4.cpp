#include "../../Header/Math/Mat4.hpp"

#include <cmath>

namespace Core
{
	Mat4 Multiply(Mat4 const& a, Mat4 const& b)
	{
		Mat4 r;
		for (int c = 0; c < 4; ++c)
		{
			for (int row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
				{
					sum += a.Values[k * 4 + row] * b.Values[c * 4 + k];
				}
				r.Values[c * 4 + row] = sum;
			}
		}
		return r;
	}

	Mat4 Translate(Vec3 t)
	{
		Mat4 r;
		r.Values[12] = t.x;
		r.Values[13] = t.y;
		r.Values[14] = t.z;
		return r;
	}

	Mat4 ScaleUniform(float s)
	{
		Mat4 r;
		r.Values[0]  = s;
		r.Values[5]  = s;
		r.Values[10] = s;
		return r;
	}

	Mat4 RotateY(float radians)
	{
		float const c = std::cos(radians);
		float const s = std::sin(radians);
		Mat4        r;
		r.Values[0]  = c;
		r.Values[2]  = -s;
		r.Values[8]  = s;
		r.Values[10] = c;
		return r;
	}

	Mat4 Perspective(float fovYRadians, float aspect, float nearPlane, float farPlane)
	{
		float const f = 1.0f / std::tan(fovYRadians * 0.5f);
		Mat4        r;
		for (float& v : r.Values)
		{
			v = 0.0f;
		}
		r.Values[0]  = f / aspect;
		r.Values[5]  = f;
		r.Values[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
		r.Values[11] = -1.0f;
		r.Values[14] = 2.0f * farPlane * nearPlane / (nearPlane - farPlane);
		return r;
	}

	Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up)
	{
		Vec3 const f = Normalize(target - eye);
		Vec3 const s = Normalize(Cross(f, up));
		Vec3 const u = Cross(s, f);
		Mat4       r;
		r.Values[0]  = s.x;
		r.Values[4]  = s.y;
		r.Values[8]  = s.z;
		r.Values[1]  = u.x;
		r.Values[5]  = u.y;
		r.Values[9]  = u.z;
		r.Values[2]  = -f.x;
		r.Values[6]  = -f.y;
		r.Values[10] = -f.z;
		r.Values[12] = -Dot(s, eye);
		r.Values[13] = -Dot(u, eye);
		r.Values[14] = Dot(f, eye);
		return r;
	}

	Mat4 Inverse(Mat4 const& m, bool& ok)
	{
		float const* a = m.Values;
		float        inv[16];

		inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
		         a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
		inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
		         a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
		inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
		         a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
		inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
		          a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
		inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
		         a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
		inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
		         a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
		inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
		         a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
		inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
		          a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
		inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
		         a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
		inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
		         a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
		inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
		          a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
		inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
		          a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
		inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
		         a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
		inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
		         a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
		inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
		          a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
		inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] +
		          a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

		float const det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
		Mat4        result;
		if (std::fabs(det) < 1e-12f)
		{
			ok = false;
			return result;
		}
		ok                 = true;
		float const invDet = 1.0f / det;
		for (int i = 0; i < 16; ++i)
		{
			result.Values[i] = inv[i] * invDet;
		}
		return result;
	}

	Vec3 TransformPoint(Mat4 const& m, Vec3 p)
	{
		float const* v = m.Values;
		return {v[0] * p.x + v[4] * p.y + v[8] * p.z + v[12], v[1] * p.x + v[5] * p.y + v[9] * p.z + v[13],
		        v[2] * p.x + v[6] * p.y + v[10] * p.z + v[14]};
	}

	Vec3 UnprojectPoint(Mat4 const& invViewProj, Vec3 ndc)
	{
		float const* v    = invViewProj.Values;
		float const  x    = v[0] * ndc.x + v[4] * ndc.y + v[8] * ndc.z + v[12];
		float const  y    = v[1] * ndc.x + v[5] * ndc.y + v[9] * ndc.z + v[13];
		float const  z    = v[2] * ndc.x + v[6] * ndc.y + v[10] * ndc.z + v[14];
		float const  w    = v[3] * ndc.x + v[7] * ndc.y + v[11] * ndc.z + v[15];
		float const  invW = std::fabs(w) > 1e-12f ? 1.0f / w : 1.0f;
		return {x * invW, y * invW, z * invW};
	}
}
