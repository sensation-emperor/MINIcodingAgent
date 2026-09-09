# Model Setup Guide - UnnatSystems Brahma Coder

## Overview

This guide covers configuration of AI model providers for UnnatSystems Brahma Coder. The application supports both **local models** (for privacy and offline use) and **cloud providers** (for maximum capability).

---

## Quick Start

### Recommended Setup for Most Users

1. **Install LM Studio** (https://lmstudio.ai/)
2. **Download a coding model:**
   - Llama-3-70B-Instruct (best overall)
   - Mixtral-8x7B-Instruct (fast, efficient)
   - CodeLlama-34B (specialized for code)
   - Phi-3-Medium (lightweight option)
3. **Start Local Server** in LM Studio (port 1234)
4. **Configure in Brahma Coder:**
   - Settings → Providers → LM Studio → Enable
   - Test connection
5. **Start coding!**

---

## Local Models

Local models run on your machine, providing:
- ✅ Complete privacy (code never leaves your computer)
- ✅ No API costs
- ✅ Offline capability
- ✅ Faster iteration for routine tasks
- ⚠️ Requires GPU/RAM resources
- ⚠️ May be less capable than largest cloud models

### Option 1: LM Studio (Recommended)

**Installation:**
```bash
# Download from https://lmstudio.ai/
# Supports Windows, macOS, Linux
```

**Setup Steps:**
1. Open LM Studio
2. Go to "Download" tab
3. Search for coding models:
   - `meta-llama/Llama-3-70B-Instruct-GGUF`
   - `mistralai/Mixtral-8x7B-Instruct-v0.1-GGUF`
   - `codellama/CodeLlama-34B-Instruct-GGUF`
4. Click Download
5. After download, go to "Local Server" tab
6. Select loaded model
7. Click "Start Server" (default port: 1234)

**Configuration in Brahma Coder:**
```json
{
  "providers": {
    "lmstudio": {
      "enabled": true,
      "endpoint": "http://localhost:1234/v1",
      "timeout_seconds": 120,
      "streaming": true
    }
  }
}
```

**Test Connection:**
```bash
curl http://localhost:1234/v1/models
```

**Expected Response:**
```json
{
  "data": [
    {
      "id": "llama-3-70b-instruct",
      "object": "model",
      "created": 1234567890
    }
  ]
}
```

### Option 2: Ollama

**Installation:**
```bash
# Linux/macOS
curl -fsSL https://ollama.com/install.sh | sh

# Windows
# Download from https://ollama.com/download
```

**Pull Models:**
```bash
ollama pull llama3:70b
ollama pull mixtral
ollama pull codellama:34b
ollama pull phi3:medium
```

**Run Server:**
```bash
ollama serve
# Default port: 11434
```

**Configuration in Brahma Coder:**
```json
{
  "providers": {
    "ollama": {
      "enabled": true,
      "endpoint": "http://localhost:11434",
      "default_model": "llama3:70b",
      "timeout_seconds": 120
    }
  }
}
```

**Test Connection:**
```bash
curl http://localhost:11434/api/tags
```

### Option 3: llama.cpp (Direct)

**Build from Source:**
```bash
git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp
make -j$(nproc)
```

**Download GGUF Model:**
```bash
# From Hugging Face
wget https://huggingface.co/TheBloke/Llama-2-70B-GGUF/resolve/main/llama-2-70b.Q4_K_M.gguf
```

**Run Server:**
```bash
./server -m llama-2-70b.Q4_K_M.gguf --port 8080 --host 0.0.0.0
```

**Configuration:**
```json
{
  "providers": {
    "llamacpp": {
      "enabled": true,
      "endpoint": "http://localhost:8080",
      "timeout_seconds": 180
    }
  }
}
```

### Option 4: TensorRT-LLM (NVIDIA GPUs)

**Prerequisites:**
- NVIDIA GPU (RTX 3090/4090 or A100/H100)
- CUDA 12.x
- Docker

**Setup:**
```bash
docker pull nvcr.io/nvidia/tensorrt-llm/release
# Follow NVIDIA documentation for model deployment
```

---

## Cloud Providers

Cloud models offer:
- ✅ Maximum capability and reasoning
- ✅ No local resource usage
- ✅ Always up-to-date
- ⚠️ Requires internet connection
- ⚠️ API costs
- ⚠️ Code sent to third-party servers

### Option 1: OpenAI

**Get API Key:**
1. Visit https://platform.openai.com/
2. Sign up / Log in
3. Go to API Keys section
4. Create new secret key
5. Copy and store securely

**Models Available:**
- `gpt-4-turbo-preview` - Best for complex reasoning
- `gpt-4` - Excellent all-around
- `gpt-3.5-turbo` - Fast and economical

**Configuration:**
```json
{
  "providers": {
    "openai": {
      "enabled": true,
      "api_key": "sk-...",
      "default_model": "gpt-4-turbo-preview",
      "base_url": "https://api.openai.com/v1",
      "timeout_seconds": 60,
      "max_tokens": 4096
    }
  }
}
```

**Environment Variable (Alternative):**
```bash
export OPENAI_API_KEY="sk-..."
```

**Test:**
```bash
curl https://api.openai.com/v1/models \
  -H "Authorization: Bearer $OPENAI_API_KEY"
```

### Option 2: Anthropic (Claude)

**Get API Key:**
1. Visit https://console.anthropic.com/
2. Sign up / Log in
3. Navigate to API Keys
4. Create new key
5. Copy securely

**Models Available:**
- `claude-3-opus-20240229` - Most powerful
- `claude-3-sonnet-20240229` - Balanced performance
- `claude-3-haiku-20240307` - Fast and efficient

**Configuration:**
```json
{
  "providers": {
    "anthropic": {
      "enabled": true,
      "api_key": "sk-ant-...",
      "default_model": "claude-3-opus-20240229",
      "base_url": "https://api.anthropic.com/v1",
      "timeout_seconds": 90,
      "max_tokens": 4096
    }
  }
}
```

**Environment Variable:**
```bash
export ANTHROPIC_API_KEY="sk-ant-..."
```

### Option 3: Google (Gemini)

**Get API Key:**
1. Visit https://makersuite.google.com/app/apikey
2. Sign in with Google account
3. Create API key
4. Copy key

**Models Available:**
- `gemini-pro` - Text-only model
- `gemini-pro-vision` - Multimodal

**Configuration:**
```json
{
  "providers": {
    "google": {
      "enabled": true,
      "api_key": "...",
      "default_model": "gemini-pro",
      "base_url": "https://generativelanguage.googleapis.com/v1beta",
      "timeout_seconds": 60
    }
  }
}
```

### Option 4: OpenRouter (Multi-Model Aggregator)

**Get API Key:**
1. Visit https://openrouter.ai/
2. Sign up
3. Create API key
4. Add credits

**Benefits:**
- Access to 50+ models through single API
- Automatic fallback between models
- Unified pricing dashboard
- No need for multiple API keys

**Models Available:**
All major models: GPT-4, Claude 3, Llama 3, Mixtral, etc.

**Configuration:**
```json
{
  "providers": {
    "openrouter": {
      "enabled": true,
      "api_key": "...",
      "default_model": "meta-llama/llama-3-70b-instruct",
      "base_url": "https://openrouter.ai/api/v1",
      "timeout_seconds": 90,
      "site_url": "https://your-site.com",
      "site_name": "Brahma Coder"
    }
  }
}
```

---

## Model Router Configuration

The Model Router automatically selects the best model based on task complexity.

### Default Routing Rules

```json
{
  "router": {
    "strategy": "role_based",
    "fallback_chain": ["lmstudio", "ollama", "openrouter", "openai"],
    "circuit_breaker": {
      "enabled": true,
      "failure_threshold": 3,
      "recovery_timeout_seconds": 30
    },
    "routing_table": {
      "planner": {
        "preferred": "llama-3-70b",
        "fallback": "gpt-4-turbo"
      },
      "researcher": {
        "preferred": "mixtral-8x7b",
        "fallback": "gpt-3.5-turbo"
      },
      "coder": {
        "preferred": "codellama-34b",
        "fallback": "gpt-4"
      },
      "reviewer": {
        "preferred": "claude-3-opus",
        "fallback": "gpt-4-turbo"
      },
      "debugger": {
        "preferred": "gpt-4-turbo",
        "fallback": "claude-3-sonnet"
      }
    }
  }
}
```

### Cost Optimization

Set spending limits:
```json
{
  "cost_manager": {
    "daily_budget_usd": 10.00,
    "monthly_budget_usd": 100.00,
    "alert_threshold_percent": 80,
    "hard_limit_enabled": true
  }
}
```

---

## Advanced Configuration

### Custom Model Endpoints

Add custom OpenAI-compatible endpoints:
```json
{
  "providers": {
    "custom_vllm": {
      "type": "openai_compatible",
      "enabled": true,
      "name": "My vLLM Server",
      "endpoint": "http://192.168.1.100:8000/v1",
      "api_key": "not-needed",
      "models": ["meta-llama/Llama-2-70b-chat-hf"]
    }
  }
}
```

### Proxy Configuration

For corporate environments:
```json
{
  "network": {
    "proxy": {
      "http": "http://proxy.company.com:8080",
      "https": "http://proxy.company.com:8080",
      "no_proxy": "localhost,127.0.0.1"
    },
    "ssl_verify": true,
    "ca_bundle_path": "/path/to/cert.pem"
  }
}
```

### Rate Limiting

Prevent API throttling:
```json
{
  "rate_limits": {
    "openai": {
      "requests_per_minute": 60,
      "tokens_per_minute": 90000
    },
    "anthropic": {
      "requests_per_minute": 50,
      "tokens_per_minute": 100000
    }
  }
}
```

---

## Troubleshooting

### Common Issues

#### "Connection refused" to local model
```bash
# Check if server is running
curl http://localhost:1234/v1/models

# Check port
netstat -tlnp | grep 1234

# Restart LM Studio server
```

#### "Invalid API key" error
```bash
# Verify key format
echo $OPENAI_API_KEY

# Test with curl
curl https://api.openai.com/v1/models \
  -H "Authorization: Bearer $OPENAI_API_KEY"

# Regenerate key if needed
```

#### Model returns gibberish
```
Solutions:
1. Ensure using instruct/chat-tuned model
2. Check temperature setting (should be 0.7-0.8)
3. Verify system prompt is correct
4. Try different model
```

#### Slow response times
```
Optimization steps:
1. Use smaller/faster model for simple tasks
2. Enable streaming responses
3. Reduce max_tokens parameter
4. Check network latency
5. Consider local model for speed
```

#### Out of memory (local models)
```
Solutions:
1. Use quantized model (Q4_K_M instead of Q8_0)
2. Reduce context window size
3. Close other GPU applications
4. Upgrade GPU VRAM
5. Use CPU offloading (--n-gpu-layers 20)
```

### Performance Tuning

#### LM Studio Settings
```
- Context length: 4096 (balance speed vs capability)
- GPU layers: Max your VRAM allows
- Batch size: 512
- Flash attention: Enabled
```

#### Optimal Model Selection by Task

| Task Type | Recommended Model | Reason |
|-----------|------------------|--------|
| Simple refactoring | Llama-3-8B | Fast, sufficient |
| Feature implementation | Llama-3-70B | Good balance |
| Complex architecture | GPT-4 Turbo / Claude 3 Opus | Maximum reasoning |
| Debugging | GPT-4 Turbo | Excellent error analysis |
| Code review | Claude 3 Sonnet | Strong at patterns |
| Documentation | Mixtral-8x7B | Fast, coherent |
| Test generation | CodeLlama-34B | Specialized |

---

## Security Best Practices

### API Key Management

✅ **DO:**
- Store keys in environment variables
- Use secret management tools (1Password, Bitwarden)
- Rotate keys periodically
- Use separate keys for development/production
- Monitor usage dashboards

❌ **DON'T:**
- Commit keys to version control
- Share keys in chat/logs
- Hardcode keys in source files
- Use same key across multiple services

### Privacy Considerations

**For sensitive codebases:**
1. Use ONLY local models
2. Disable all cloud providers
3. Enable air-gap mode
4. Review tool permissions
5. Audit conversation logs

**Configuration for air-gap:**
```json
{
  "security": {
    "air_gap_mode": true,
    "allowed_providers": ["lmstudio", "ollama"],
    "blocked_providers": ["openai", "anthropic", "google"],
    "audit_logging": true
  }
}
```

---

## Monitoring & Metrics

### View Usage Statistics

In GUI: Settings → Usage Dashboard

Or via CLI:
```bash
brahma_coder metrics show --period today
brahma_coder metrics show --period week
brahma_coder metrics export --format csv
```

### Alerting

Configure alerts:
```json
{
  "alerts": {
    "email": "you@example.com",
    "slack_webhook": "https://hooks.slack.com/...",
    "thresholds": {
      "daily_cost_usd": 5.00,
      "error_rate_percent": 5.0,
      "latency_p99_ms": 5000
    }
  }
}
```

---

## Updates & Maintenance

### Check for Model Updates

```bash
# LM Studio
lmstudio check-updates

# Ollama
ollama pull llama3:70b  # Re-pull for latest

# Cloud providers
# Check provider documentation for new models
```

### Update Configuration

After adding new providers:
```bash
brahma_coder config reload
brahma_coder providers test-all
```

---

*Last updated: 2026-09-09*
