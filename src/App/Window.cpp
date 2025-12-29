#include "Window.h"

#include <QLabel>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <ctime>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tinygltf/tiny_gltf.h>

namespace
{
// xy coords + tex coords
constexpr std::array<GLfloat, 4 * 4> vertices = {
	-1.0f,
	-1.0f,
	0.f,
	0.f,
	1.0f,
	-1.0f,
	1.f,
	0.f,
	1.0f,
	1.0f,
	1.f,
	1.f,
	-1.0f,
	1.0f,
	0.f,
	1.f,
};

constexpr std::array<GLuint, 6u> indices = {0, 1, 2, 0, 2, 3};
}// namespace

Window::Window() noexcept
{
	const auto formatFPS = [](const auto value) {
		return QString("FPS: %1").arg(QString::number(value));
	};

	auto fps = new QLabel(formatFPS(0), this);
	fps->setStyleSheet("QLabel { color : white; }");

	auto layout = new QVBoxLayout();
	layout->addWidget(fps, 1);

	setLayout(layout);

	auto controlLayout = new QHBoxLayout();

	timer_.start();

	connect(this, &Window::updateUI, [=] {
		fps->setText(formatFPS(ui_.fps));
	});

	iterationsSlider_ = new QSlider(Qt::Horizontal, this);
	iterationsSlider_->setRange(1, 60);
	iterationsSlider_->setValue(30);
	iterationsSlider_->setMaximumWidth(150);

	redSlider_ = new QSlider(Qt::Horizontal, this);
	redSlider_->setRange(0, 100);
	redSlider_->setValue(22);
	redSlider_->setMaximumWidth(100);

	greenSlider_ = new QSlider(Qt::Horizontal, this);
	greenSlider_->setRange(0, 100);
	greenSlider_->setValue(30);
	greenSlider_->setMaximumWidth(100);

	blueSlider_ = new QSlider(Qt::Horizontal, this);
	blueSlider_->setRange(0, 100);
	blueSlider_->setValue(90);
	blueSlider_->setMaximumWidth(100);

	QLabel * iterationsLabel = new QLabel("Iterations:", this);
	iterationsLabel->setStyleSheet("color: white;");

	QLabel * redLabel = new QLabel("R:", this);
	redLabel->setStyleSheet("color: white;");

	QLabel * greenLabel = new QLabel("G:", this);
	greenLabel->setStyleSheet("color: white;");

	QLabel * blueLabel = new QLabel("B:", this);
	blueLabel->setStyleSheet("color: white;");

	controlLayout->addWidget(iterationsLabel);
	controlLayout->addWidget(iterationsSlider_);
	controlLayout->addWidget(redLabel);
	controlLayout->addWidget(redSlider_);
	controlLayout->addWidget(greenLabel);
	controlLayout->addWidget(greenSlider_);
	controlLayout->addWidget(blueLabel);
	controlLayout->addWidget(blueSlider_);

	layout->addLayout(controlLayout);

	connect(iterationsSlider_, &QSlider::valueChanged, [this](int value) {
		iterationsValue_ = static_cast<float>(value);
		update();
	});

	auto updateColor = [this]() {
		colorValue_ = QVector3D(
			redSlider_->value() / 100.0f,
			greenSlider_->value() / 100.0f,
			blueSlider_->value() / 100.0f);
		update();
	};

	connect(redSlider_, &QSlider::valueChanged, updateColor);
	connect(greenSlider_, &QSlider::valueChanged, updateColor);
	connect(blueSlider_, &QSlider::valueChanged, updateColor);
}

Window::~Window()
{
	{
		// Free resources with context bounded.
		const auto guard = bindContext();
		program_.reset();
	}
}

void Window::onInit()
{
	// Configure shaders
	program_ = std::make_unique<QOpenGLShaderProgram>(this);
	program_->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/Shaders/diffuse.vs");
	program_->addShaderFromSourceFile(QOpenGLShader::Fragment,
									  ":/Shaders/diffuse.fs");
	program_->link();

	// Create VAO object
	vao_.create();
	vao_.bind();

	// Create VBO
	vbo_.create();
	vbo_.bind();
	vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo_.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(GLfloat)));

	// Create IBO
	ibo_.create();
	ibo_.bind();
	ibo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	ibo_.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(GLuint)));

	// Bind attributes
	program_->bind();

	program_->enableAttributeArray(0);
	program_->setAttributeBuffer(0, GL_FLOAT, 0, 2, static_cast<int>(4 * sizeof(GLfloat)));

	program_->enableAttributeArray(1);
	program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(2 * sizeof(GLfloat)), 2,
								 static_cast<int>(4 * sizeof(GLfloat)));

	mvpUniform_ = program_->uniformLocation("mvp");
	iterationsUniform_ = program_->uniformLocation("iterations");
	colorUniform_ = program_->uniformLocation("colorMult");
	positionUniform_ = program_->uniformLocation("pos");
	zoomUniform_ = program_->uniformLocation("zoomLog");

	// Release all
	program_->release();

	vao_.release();

	ibo_.release();
	vbo_.release();

	// Еnable depth test and face culling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glClearColor(0.30f, 0.30f, 0.30f, 1.0f);

	// Clear all FBO buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::onRender()
{
	const auto guard = captureMetrics();

	// Clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Calculate MVP matrix
	model_.setToIdentity();
	model_.translate(0, 0, 0);
	view_.setToIdentity();
	const auto mvp = projection_ * view_ * model_;

	// Bind VAO and shader program
	program_->bind();
	vao_.bind();

	// Update uniform value
	program_->setUniformValue(mvpUniform_, mvp);
	program_->setUniformValue(iterationsUniform_, iterationsValue_);
	program_->setUniformValue(colorUniform_, colorValue_);

	program_->setUniformValue(iterationsUniform_, iterationsValue_);
	program_->setUniformValue(colorUniform_, colorValue_);
	program_->setUniformValue(positionUniform_, modelCenterPos_);
	program_->setUniformValue(zoomUniform_, zoomLog_);

	// Draw
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);

	// Release VAO and shader program
	vao_.release();
	program_->release();

	++frameCount_;

	// Request redraw if animated
	if (animated_)
	{
		update();
	}
}

void Window::onResize(const size_t width, const size_t height)
{
	// Configure viewport
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));

	// Configure matrix
	// const auto aspect = static_cast<float>(width) / static_cast<float>(height);
	// const auto zNear = 0.1f;
	// const auto zFar = 100.0f;
	// const auto fov = 60.0f;
	// projection_.setToIdentity();
	// projection_.perspective(fov, aspect, zNear, zFar);

	float w = (float)width;
	float h = (float)height;
	float ww, hh;
	if (w > h)
	{
		ww = w / h;
		hh = 1;
	}
	else
	{
		ww = 1;
		hh = h / w;
	}

	projection_.setToIdentity();
	projection_.ortho(-ww, ww, -hh, hh, -1.0, 1.0);
}

void Window::mousePressEvent(QMouseEvent * event)
{
	isPressed_ = true;
	lastMousePos_ = event->pos();
}

void Window::mouseMoveEvent(QMouseEvent * event)
{
	if (isPressed_)
	{
		QVector2D delta = toModelCoords(event->pos()) - toModelCoords(lastMousePos_);
		lastMousePos_ = event->pos();

		modelCenterPos_ -= delta / exp(zoomLog_);
	}
}
void Window::mouseReleaseEvent([[maybe_unused]] QMouseEvent * event)
{
	isPressed_ = false;
}

QVector2D Window::toModelCoords(QPoint p)
{
	float scale = std::min(width(), height());

	return QVector2D(
		2.0f * ((float)p.x() - width() / 2.0f) / scale,
		2.0f * (height() / 2.0f - (float)p.y()) / scale);
}

void Window::wheelEvent(QWheelEvent * event)
{
	float deltaZoomLog = event->angleDelta().y() / 1000.0f;
	float newZoomLog = std::clamp(zoomLog_ + deltaZoomLog, -15.0f, 15.0f);

	QVector2D modelMoucePos = modelCenterPos_ + toModelCoords(event->pos()) / exp(zoomLog_);
	modelCenterPos_ = modelMoucePos + (modelCenterPos_ - modelMoucePos) / exp(newZoomLog - zoomLog_);
	zoomLog_ = newZoomLog;
}

Window::PerfomanceMetricsGuard::PerfomanceMetricsGuard(std::function<void()> callback)
	: callback_{std::move(callback)}
{
}

Window::PerfomanceMetricsGuard::~PerfomanceMetricsGuard()
{
	if (callback_)
	{
		callback_();
	}
}

auto Window::captureMetrics() -> PerfomanceMetricsGuard
{
	return PerfomanceMetricsGuard{
		[&] {
			if (timer_.elapsed() >= 1000)
			{
				const auto elapsedSeconds = static_cast<float>(timer_.restart()) / 1000.0f;
				ui_.fps = static_cast<size_t>(std::round(frameCount_ / elapsedSeconds));
				frameCount_ = 0;
				emit updateUI();
			}
		}
	};
}
