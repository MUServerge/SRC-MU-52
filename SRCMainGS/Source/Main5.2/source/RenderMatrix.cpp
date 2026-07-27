// RenderMatrix - see RenderMatrix.h. Standalone (no stdafx / OpenGL) so it
// builds identically under MSVC and any host compiler and stays unit-testable.

#include "RenderMatrix.h"
#include <cmath>

namespace RenderMatrix
{
	namespace
	{
		// Column-major index of element (row i, col j).
		inline int Idx(int row, int col) { return col * 4 + row; }

		void Vec3Sub(const float a[3], const float b[3], float out[3])
		{
			out[0] = a[0] - b[0];
			out[1] = a[1] - b[1];
			out[2] = a[2] - b[2];
		}

		void Vec3Cross(const float a[3], const float b[3], float out[3])
		{
			out[0] = a[1] * b[2] - a[2] * b[1];
			out[1] = a[2] * b[0] - a[0] * b[2];
			out[2] = a[0] * b[1] - a[1] * b[0];
		}

		float Vec3Dot(const float a[3], const float b[3])
		{
			return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
		}

		// Normalize in place; returns false (leaving the vector untouched) when
		// its length is degenerate.
		bool Vec3Normalize(float v[3])
		{
			const float lengthSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
			if (lengthSq <= 1e-20f)
				return false;
			const float inv = 1.0f / sqrtf(lengthSq);
			v[0] *= inv;
			v[1] *= inv;
			v[2] *= inv;
			return true;
		}

		const float kPi = 3.14159265358979323846f;
	}

	void Identity(float m[16])
	{
		for (int i = 0; i < 16; ++i)
			m[i] = 0.0f;
		m[0] = m[5] = m[10] = m[15] = 1.0f;
	}

	void Copy(float dst[16], const float src[16])
	{
		for (int i = 0; i < 16; ++i)
			dst[i] = src[i];
	}

	void Multiply(float out[16], const float a[16], const float b[16])
	{
		// out(i,j) = sum_k a(i,k) * b(k,j), column-major throughout.
		for (int col = 0; col < 4; ++col)
		{
			for (int row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
					sum += a[Idx(row, k)] * b[Idx(k, col)];
				out[Idx(row, col)] = sum;
			}
		}
	}

	void MultiplyRight(float m[16], const float b[16])
	{
		float result[16];
		Multiply(result, m, b);
		Copy(m, result);
	}

	void Translate(float m[16], float x, float y, float z)
	{
		float t[16];
		Identity(t);
		t[Idx(0, 3)] = x;
		t[Idx(1, 3)] = y;
		t[Idx(2, 3)] = z;
		MultiplyRight(m, t);
	}

	void Scale(float m[16], float x, float y, float z)
	{
		float s[16];
		Identity(s);
		s[Idx(0, 0)] = x;
		s[Idx(1, 1)] = y;
		s[Idx(2, 2)] = z;
		MultiplyRight(m, s);
	}

	void Rotate(float m[16], float angleDegrees, float x, float y, float z)
	{
		float axis[3] = { x, y, z };
		if (!Vec3Normalize(axis))
			return; // degenerate axis: leave m unchanged, like a no-op rotate

		const float radians = angleDegrees * (kPi / 180.0f);
		const float c = cosf(radians);
		const float s = sinf(radians);
		const float oneMinusC = 1.0f - c;
		const float ax = axis[0];
		const float ay = axis[1];
		const float az = axis[2];

		float r[16];
		Identity(r);
		// Standard glRotate rotation matrix (column-major).
		r[Idx(0, 0)] = ax * ax * oneMinusC + c;
		r[Idx(1, 0)] = ay * ax * oneMinusC + az * s;
		r[Idx(2, 0)] = ax * az * oneMinusC - ay * s;

		r[Idx(0, 1)] = ax * ay * oneMinusC - az * s;
		r[Idx(1, 1)] = ay * ay * oneMinusC + c;
		r[Idx(2, 1)] = ay * az * oneMinusC + ax * s;

		r[Idx(0, 2)] = ax * az * oneMinusC + ay * s;
		r[Idx(1, 2)] = ay * az * oneMinusC - ax * s;
		r[Idx(2, 2)] = az * az * oneMinusC + c;

		MultiplyRight(m, r);
	}

	void Frustum(float m[16], float left, float right, float bottom, float top,
		float zNear, float zFar)
	{
		for (int i = 0; i < 16; ++i)
			m[i] = 0.0f;

		const float rl = right - left;
		const float tb = top - bottom;
		const float fn = zFar - zNear;

		m[Idx(0, 0)] = (2.0f * zNear) / rl;
		m[Idx(1, 1)] = (2.0f * zNear) / tb;
		m[Idx(0, 2)] = (right + left) / rl;
		m[Idx(1, 2)] = (top + bottom) / tb;
		m[Idx(2, 2)] = -(zFar + zNear) / fn;
		m[Idx(3, 2)] = -1.0f;
		m[Idx(2, 3)] = -(2.0f * zFar * zNear) / fn;
	}

	void Ortho(float m[16], float left, float right, float bottom, float top,
		float zNear, float zFar)
	{
		Identity(m);

		const float rl = right - left;
		const float tb = top - bottom;
		const float fn = zFar - zNear;

		m[Idx(0, 0)] = 2.0f / rl;
		m[Idx(1, 1)] = 2.0f / tb;
		m[Idx(2, 2)] = -2.0f / fn;
		m[Idx(0, 3)] = -(right + left) / rl;
		m[Idx(1, 3)] = -(top + bottom) / tb;
		m[Idx(2, 3)] = -(zFar + zNear) / fn;
	}

	void Perspective(float m[16], float fovYDegrees, float aspect,
		float zNear, float zFar)
	{
		for (int i = 0; i < 16; ++i)
			m[i] = 0.0f;

		const float f = 1.0f / tanf((fovYDegrees * (kPi / 180.0f)) * 0.5f);
		const float nf = zNear - zFar;

		m[Idx(0, 0)] = f / aspect;
		m[Idx(1, 1)] = f;
		m[Idx(2, 2)] = (zFar + zNear) / nf;
		m[Idx(3, 2)] = -1.0f;
		m[Idx(2, 3)] = (2.0f * zFar * zNear) / nf;
	}

	void LookAt(float m[16], const float eye[3], const float center[3],
		const float up[3])
	{
		float forward[3];
		Vec3Sub(center, eye, forward);
		if (!Vec3Normalize(forward))
		{
			Identity(m);
			return;
		}

		float side[3];
		Vec3Cross(forward, up, side);
		Vec3Normalize(side);

		float trueUp[3];
		Vec3Cross(side, forward, trueUp);

		Identity(m);
		m[Idx(0, 0)] = side[0];
		m[Idx(0, 1)] = side[1];
		m[Idx(0, 2)] = side[2];

		m[Idx(1, 0)] = trueUp[0];
		m[Idx(1, 1)] = trueUp[1];
		m[Idx(1, 2)] = trueUp[2];

		m[Idx(2, 0)] = -forward[0];
		m[Idx(2, 1)] = -forward[1];
		m[Idx(2, 2)] = -forward[2];

		m[Idx(0, 3)] = -Vec3Dot(side, eye);
		m[Idx(1, 3)] = -Vec3Dot(trueUp, eye);
		m[Idx(2, 3)] = Vec3Dot(forward, eye);
	}

	// --- Stack ---------------------------------------------------------------

	Stack::Stack()
		: m_Depth(1)
	{
		Identity(m_Stack[0]);
	}

	void Stack::LoadIdentity()
	{
		Identity(m_Stack[m_Depth - 1]);
	}

	void Stack::Load(const float m[16])
	{
		Copy(m_Stack[m_Depth - 1], m);
	}

	bool Stack::Push()
	{
		if (m_Depth >= CAPACITY)
			return false;
		Copy(m_Stack[m_Depth], m_Stack[m_Depth - 1]);
		++m_Depth;
		return true;
	}

	bool Stack::Pop()
	{
		if (m_Depth <= 1)
			return false;
		--m_Depth;
		return true;
	}

	void Stack::MultiplyRight(const float m[16])
	{
		RenderMatrix::MultiplyRight(m_Stack[m_Depth - 1], m);
	}

	void Stack::Translate(float x, float y, float z)
	{
		RenderMatrix::Translate(m_Stack[m_Depth - 1], x, y, z);
	}

	void Stack::Scale(float x, float y, float z)
	{
		RenderMatrix::Scale(m_Stack[m_Depth - 1], x, y, z);
	}

	void Stack::Rotate(float angleDegrees, float x, float y, float z)
	{
		RenderMatrix::Rotate(m_Stack[m_Depth - 1], angleDegrees, x, y, z);
	}
}
