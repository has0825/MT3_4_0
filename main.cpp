#include <Novice.h>
#include <cmath>
#include <imgui.h>

const char kWindowTitle[] = "X方向ばねの小さな伸縮";

struct Vector3 {
	float x, y, z;
};

struct Matrix4x4 {
	float m[4][4];
};

struct Spring {
	Vector3 anchor;
	float naturalLength;
	float stiffness;
	float dampingCoefficient;
};

struct Ball {
	Vector3 position;
	Vector3 velocity;
	float mass;
	float radius;
	unsigned int color;
};

Vector3 Transform(const Vector3& v, const Matrix4x4& m) {
	float x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
	float y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
	float z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
	float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
	if (w != 0.0f) {
		x /= w; y /= w; z /= w;
	}
	return { x, y, z };
}

Vector3 Normalize(const Vector3& v) {
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return { v.x / length, v.y / length, v.z / length };
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			for (int k = 0; k < 4; ++k)
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
	return result;
}

Matrix4x4 MakeRotateMatrix(const Vector3& rotate) {
	float cosX = cosf(rotate.x), sinX = sinf(rotate.x);
	float cosY = cosf(rotate.y), sinY = sinf(rotate.y);
	float cosZ = cosf(rotate.z), sinZ = sinf(rotate.z);

	Matrix4x4 rotX = { 1,0,0,0, 0,cosX,sinX,0, 0,-sinX,cosX,0, 0,0,0,1 };
	Matrix4x4 rotY = { cosY,0,-sinY,0, 0,1,0,0, sinY,0,cosY,0, 0,0,0,1 };
	Matrix4x4 rotZ = { cosZ,sinZ,0,0, -sinZ,cosZ,0,0, 0,0,1,0, 0,0,0,1 };

	return Multiply(Multiply(rotZ, rotX), rotY);
}

Matrix4x4 MakeViewMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
	Vector3 z = Normalize({ target.x - eye.x, target.y - eye.y, target.z - eye.z });
	Vector3 x = Normalize(Cross(up, z));
	Vector3 y = Cross(z, x);
	Matrix4x4 m = {};
	m.m[0][0] = x.x; m.m[1][0] = x.y; m.m[2][0] = x.z; m.m[3][0] = -x.x * eye.x - x.y * eye.y - x.z * eye.z;
	m.m[0][1] = y.x; m.m[1][1] = y.y; m.m[2][1] = y.z; m.m[3][1] = -y.x * eye.x - y.y * eye.y - y.z * eye.z;
	m.m[0][2] = z.x; m.m[1][2] = z.y; m.m[2][2] = z.z; m.m[3][2] = -z.x * eye.x - z.y * eye.y - z.z * eye.z;
	m.m[0][3] = 0;   m.m[1][3] = 0;   m.m[2][3] = 0;   m.m[3][3] = 1;
	return m;
}

Matrix4x4 MakePerspectiveMatrix(float fovY, float aspect, float nearZ, float farZ) {
	float f = 1.0f / tanf(fovY / 2.0f);
	Matrix4x4 m{};
	m.m[0][0] = f / aspect;
	m.m[1][1] = f;
	m.m[2][2] = farZ / (farZ - nearZ);
	m.m[2][3] = 1.0f;
	m.m[3][2] = -nearZ * farZ / (farZ - nearZ);
	return m;
}

void DrawGrid(const Matrix4x4& vp, const Matrix4x4& viewport) {
	float hw = 2.0f;
	int div = 10;
	float step = (hw * 2.0f) / div;
	for (int i = 0; i <= div; i++) {
		float offset = -hw + step * i;
		Vector3 sx = Transform(Transform({ offset, 0, -hw }, vp), viewport);
		Vector3 ex = Transform(Transform({ offset, 0, hw }, vp), viewport);
		Vector3 sz = Transform(Transform({ -hw, 0, offset }, vp), viewport);
		Vector3 ez = Transform(Transform({ hw, 0, offset }, vp), viewport);
		int color = (offset == 0) ? 0x000000FF : 0xAAAAAAFF;
		Novice::DrawLine(int(sx.x), int(sx.y), int(ex.x), int(ex.y), color);
		Novice::DrawLine(int(sz.x), int(sz.y), int(ez.x), int(ez.y), color);
	}
}

Vector3 cameraTranslate = { 0, 2, -6 };
Vector3 cameraRotate = { 0, 0, 0 };

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Novice::Initialize(kWindowTitle, 1280, 720);
	char keys[256] = {}, preKeys[256] = {};

	Spring spring = { {0,0,0}, 1.0f, 100.0f, 2.0f };
	Ball ball = { {1.20f,0,0}, {0,0,0}, 2.0f, 0.05f, 0x0000FFFF };

	bool isMoving = false;

	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);

		// ImGui
		ImGui::Begin("Control");
		ImGui::DragFloat3("CameraTranslate", &cameraTranslate.x, 0.01f);
		ImGui::DragFloat3("CameraRotate", &cameraRotate.x, 0.01f);
		if (ImGui::Button("Start Spring")) isMoving = true;
		if (ImGui::Button("Reset")) {
			ball.position = { 1.20f, 0, 0 };
			ball.velocity = { 0, 0, 0 };
			isMoving = false;
		}
		ImGui::End();

		// 物理更新
		if (isMoving) {
			const float dt = 1.0f / 60.0f;

			Vector3 delta = {
				ball.position.x - spring.anchor.x,
				ball.position.y - spring.anchor.y,
				ball.position.z - spring.anchor.z
			};
			float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
			Vector3 dir = Normalize(delta);
			float stretch = length - spring.naturalLength;

			Vector3 springForce = {
				-spring.stiffness * stretch * dir.x,
				-spring.stiffness * stretch * dir.y,
				-spring.stiffness * stretch * dir.z
			};
			Vector3 dampingForce = {
				-spring.dampingCoefficient * ball.velocity.x,
				-spring.dampingCoefficient * ball.velocity.y,
				-spring.dampingCoefficient * ball.velocity.z
			};
			Vector3 force = {
				springForce.x + dampingForce.x,
				springForce.y + dampingForce.y,
				springForce.z + dampingForce.z
			};
			Vector3 acc = {
				force.x / ball.mass,
				force.y / ball.mass,
				force.z / ball.mass
			};

			ball.velocity.x += acc.x * dt;
			ball.velocity.y += acc.y * dt;
			ball.velocity.z += acc.z * dt;

			ball.position.x += ball.velocity.x * dt;
			ball.position.y += ball.velocity.y * dt;
			ball.position.z += ball.velocity.z * dt;
		}

		// 行列計算
		Matrix4x4 rot = MakeRotateMatrix(cameraRotate);
		Vector3 forward = Transform({ 0,0,1 }, rot);
		Vector3 target = {
			cameraTranslate.x + forward.x,
			cameraTranslate.y + forward.y,
			cameraTranslate.z + forward.z
		};
		Vector3 up = Transform({ 0,1,0 }, rot);
		Matrix4x4 view = MakeViewMatrix(cameraTranslate, target, up);
		Matrix4x4 proj = MakePerspectiveMatrix(0.5f, 1280.0f / 720.0f, 0.1f, 100.0f);
		Matrix4x4 vp = Multiply(view, proj);
		Matrix4x4 viewport = {
			640,0,0,0, 0,-360,0,0, 0,0,1,0, 640,360,0,1
		};

		// 描画
		DrawGrid(vp, viewport);
		Vector3 screenAnchor = Transform(Transform(spring.anchor, vp), viewport);
		Vector3 screenBall = Transform(Transform(ball.position, vp), viewport);
		Novice::DrawEllipse(int(screenBall.x), int(screenBall.y), 10, 10, 0.0f, ball.color, kFillModeSolid);
		Novice::DrawLine(int(screenAnchor.x), int(screenAnchor.y), int(screenBall.x), int(screenBall.y), 0xFF0000FF);

		Novice::EndFrame();

		if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) break;
	}
	Novice::Finalize();
	return 0;
}
