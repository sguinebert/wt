export interface RuntimeConfig {
  actionEndpoint: string;
}

const DEFAULT_CONFIG: RuntimeConfig = {
  actionEndpoint: '/_wt/action',
};

export async function postAction(config: RuntimeConfig, action: string, nodeId: string): Promise<void> {
  await fetch(config.actionEndpoint, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
    },
    body: JSON.stringify({
      action,
      nodeId,
      ts: Date.now(),
    }),
    keepalive: true,
  });
}

export function loadRuntimeConfig(scriptId = 'wt-qml-static-config'): RuntimeConfig {
  const script = document.getElementById(scriptId);
  if (!script?.textContent) {
    return DEFAULT_CONFIG;
  }

  try {
    const parsed = JSON.parse(script.textContent) as Partial<RuntimeConfig>;
    return {
      actionEndpoint: parsed.actionEndpoint || DEFAULT_CONFIG.actionEndpoint,
    };
  } catch {
    return DEFAULT_CONFIG;
  }
}

export function installDefaultActionBridge(config: RuntimeConfig): void {
  document.addEventListener('click', (event: Event) => {
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
      return;
    }
    const actionNode = target.closest<HTMLElement>('[data-wt-action]');
    if (!actionNode?.dataset.wtAction) {
      return;
    }
    void postAction(config, actionNode.dataset.wtAction, actionNode.id);
  });
}

export function bootstrapFromDocument(): void {
  const config = loadRuntimeConfig();
  installDefaultActionBridge(config);
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => bootstrapFromDocument(), { once: true });
} else {
  bootstrapFromDocument();
}
