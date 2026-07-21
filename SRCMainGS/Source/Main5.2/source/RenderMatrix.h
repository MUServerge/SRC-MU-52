#pragma once

// ---------------------------------------------------------------------------
// RenderMatrix - OpenGL 3.3 migration matrix backbone (Phase 13).
//
// A CPU replacement for the fixed-function matrix stack (glMatrixMode,
// glPushMatrix/glPopMatrix, glLoadMatrix/glMultMatrix, glTranslate/glRotate/
// glScale, gluPerspective/glFrustum/glOrtho, gluLookAt), none of which exist
// in an OpenGL Core profile.
//
// Storage and conventions are identical to OpenGL: a matrix is 16 floats in
// COLUMN-MAJOR order, so element (row i, col j) lives at index j*4 + i. Output
// matrices can be handed directly to glUniformMatrix4fv(..., GL_FALSE, m) and
// consumed by the existing `uProj`/`uView` shaders (Client\Data\Effect\VBO).
//
// This module is intentionally standalone (no OpenGL, no stdafx, no Windows
// headers) so it is unit-testable on any host and cannot regress the renderer:
// Phase 13 only introduces it; later phases adopt it in place of the calls
// above. The projection/frustum/ortho/lookAt builders reproduce the exact GL
// and GLU formulas so the produced matrices match the current pipeline 1:1.
// ---------------------------------------------------------------------------

namespace RenderMatrix
{
	// --- Free builders. All operate on a column-major float[16]. -------------

	// Set m to the identity matrix.
	void Identity(float m[16]);

	// dst = src.
	void Copy(float dst[16], const float src[16]);

	// out = a * b, matching OpenGL's column-major product. `out` may alias
	// neither `a` nor `b`; use MultiplyInPlace for the aliasing case.
	void Multiply(float out[16], const float a[16], const float b[16]);

	// m = m * b (post-multiply, the semantics of glMultMatrix / glTranslate /
	// glRotate / glScale). Safe when b aliases nothing; m is updated in place.
	void MultiplyRight(float m[16], const float b[16]);

	// Post-multiply m by the corresponding transform (m = m * T), exactly like
	// the fixed-function glTranslate/glScale/glRotate applied to the current
	// matrix. Rotate takes its angle in degrees around the given axis, which is
	// normalized internally (a zero-length axis leaves m unchanged).
	void Translate(float m[16], float x, float y, float z);
	void Scale(float m[16], float x, float y, float z);
	void Rotate(float m[16], float angleDegrees, float x, float y, float z);

	// Load a projection into m (replaces m, i.e. glLoadIdentity + glFrustum /
	// glOrtho). Arguments follow glFrustum / glOrtho exactly.
	void Frustum(float m[16], float left, float right, float bottom, float top,
		float zNear, float zFar);
	void Ortho(float m[16], float left, float right, float bottom, float top,
		float zNear, float zFar);

	// Load a gluPerspective projection into m. `fovYDegrees` is the vertical
	// field of view in degrees, matching gluPerspective / gluPerspective2.
	void Perspective(float m[16], float fovYDegrees, float aspect,
		float zNear, float zFar);

	// Load a gluLookAt view matrix into m. eye/center/up are 3-float vectors.
	void LookAt(float m[16], const float eye[3], const float center[3],
		const float up[3]);

	// --- Matrix stack: a direct replacement for the GL matrix stack. ---------
	// Construction leaves depth 1 with the identity on top. Push duplicates the
	// current top; Pop discards it (never below depth 1). The transform helpers
	// post-multiply the current top, matching glTranslate/glRotate/glScale/
	// glMultMatrix against the active matrix.

	class Stack
	{
	public:
		enum { CAPACITY = 32 };

		Stack();

		void LoadIdentity();
		void Load(const float m[16]);

		bool Push();          // duplicate top; false if capacity reached
		bool Pop();           // false if already at the base level
		int  Depth() const { return m_Depth; }

		const float* Top() const { return m_Stack[m_Depth - 1]; }
		float*       Top()       { return m_Stack[m_Depth - 1]; }

		void MultiplyRight(const float m[16]);
		void Translate(float x, float y, float z);
		void Scale(float x, float y, float z);
		void Rotate(float angleDegrees, float x, float y, float z);

	private:
		float m_Stack[CAPACITY][16];
		int   m_Depth;
	};
}
