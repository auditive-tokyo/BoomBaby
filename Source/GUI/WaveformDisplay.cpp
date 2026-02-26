#include "WaveformDisplay.h"
#include "UIConstants.h"

// ── GLSL シェーダソース ──

static constexpr const char *const vertexShaderSource = R"(
  attribute vec2 position;
  void main() {
    gl_Position = vec4(position, 0.0, 1.0);
  }
)";

static constexpr const char *const fragmentShaderSource = R"(
  uniform vec4 waveColour;
  void main() {
    gl_FragColor = waveColour;
  }
)";

// ──────────────────────────────────────────

WaveformDisplay::WaveformDisplay() {
  displayBuffer.resize(maxDisplaySamples, 0.0f);
  renderBuffer.resize(static_cast<size_t>(maxDisplaySamples) * 2, 0.0f);

  glContext.setRenderer(this);
  glContext.setContinuousRepainting(true);
  glContext.attachTo(*this);
}

WaveformDisplay::~WaveformDisplay() { glContext.detach(); }

void WaveformDisplay::updateWaveform(const float *data, int numSamples) {
  if (data == nullptr || numSamples <= 0)
    return;

  const int count = std::min(numSamples, maxDisplaySamples);
  const std::lock_guard lock(dataMutex);
  std::copy_n(data, count, displayBuffer.begin());

  // サンプル数が足りない場合は残りをゼロ埋め
  if (count < maxDisplaySamples)
    std::fill(displayBuffer.begin() + count, displayBuffer.end(), 0.0f);

  dataReady = true;
}

// ── OpenGLRenderer ──

void WaveformDisplay::newOpenGLContextCreated() {
  // VAO（macOS Core Profile で必須）
  juce::gl::glGenVertexArrays(1, &vao);
  juce::gl::glBindVertexArray(vao);

  // VBO
  juce::gl::glGenBuffers(1, &vbo);
  juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);

  // 初期確保（vec2 × maxDisplaySamples）
  const auto bufferBytes =
      static_cast<GLsizeiptr>(maxDisplaySamples * 2 * sizeof(float));
  juce::gl::glBufferData(juce::gl::GL_ARRAY_BUFFER, bufferBytes, nullptr,
                         juce::gl::GL_DYNAMIC_DRAW);

  juce::gl::glBindVertexArray(0);

  buildShader();
}

void WaveformDisplay::renderOpenGL() {
  // 背景クリア
  juce::OpenGLHelpers::clear(UIConstants::Colours::waveformBg);

  if (shader == nullptr)
    return;

  // mutex で displayBuffer → renderBuffer にコピー（最小限のロック時間）
  {
    const std::lock_guard lock(dataMutex);
    if (!dataReady)
      return; // まだデータが来ていない
    // vec2 配列を構築: x = -1..+1, y = sample value
    const float xStep = 2.0f / static_cast<float>(maxDisplaySamples - 1);
    for (int i = 0; i < maxDisplaySamples; ++i) {
      const auto idx = static_cast<size_t>(i) * 2;
      renderBuffer[idx] = -1.0f + xStep * static_cast<float>(i);     // x
      renderBuffer[idx + 1] = displayBuffer[static_cast<size_t>(i)]; // y
    }
  }

  // VBO にアップロード
  juce::gl::glBindVertexArray(vao);
  juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, vbo);
  const auto dataBytes =
      static_cast<GLsizeiptr>(maxDisplaySamples * 2 * sizeof(float));
  juce::gl::glBufferSubData(juce::gl::GL_ARRAY_BUFFER, 0, dataBytes,
                            renderBuffer.data());

  // シェーダ適用
  shader->use();

  // 波形色（subArc から取得）
  if (const auto colourLocation =
          juce::gl::glGetUniformLocation(shader->getProgramID(), "waveColour");
      colourLocation >= 0) {
    const auto c = UIConstants::Colours::subArc;
    juce::gl::glUniform4f(colourLocation,
                          c.getFloatRed(), c.getFloatGreen(),
                          c.getFloatBlue(), 1.0f);
  }

  // 頂点アトリビュート
  if (const auto posAttrib =
          juce::gl::glGetAttribLocation(shader->getProgramID(), "position");
      posAttrib >= 0) {
    juce::gl::glEnableVertexAttribArray(static_cast<GLuint>(posAttrib));
    juce::gl::glVertexAttribPointer(static_cast<GLuint>(posAttrib), 2,
                                    juce::gl::GL_FLOAT, juce::gl::GL_FALSE,
                                    2 * sizeof(float), nullptr);

    // LINE_STRIP で描画
    juce::gl::glLineWidth(2.0f);
    juce::gl::glDrawArrays(juce::gl::GL_LINE_STRIP, 0, maxDisplaySamples);

    juce::gl::glDisableVertexAttribArray(static_cast<GLuint>(posAttrib));
  }

  juce::gl::glBindBuffer(juce::gl::GL_ARRAY_BUFFER, 0);
  juce::gl::glBindVertexArray(0);
}

void WaveformDisplay::openGLContextClosing() {
  shader.reset();

  if (vbo != 0) {
    juce::gl::glDeleteBuffers(1, &vbo);
    vbo = 0;
  }
  if (vao != 0) {
    juce::gl::glDeleteVertexArrays(1, &vao);
    vao = 0;
  }
}

void WaveformDisplay::buildShader() {
  shader = std::make_unique<juce::OpenGLShaderProgram>(glContext);

  if (!shader->addVertexShader(vertexShaderSource) ||
      !shader->addFragmentShader(fragmentShaderSource) || !shader->link()) {
    DBG("WaveformDisplay: shader compile/link failed");
    shader.reset();
  }
}
