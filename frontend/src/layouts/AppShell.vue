<template>
  <div class="app-shell">
    <header class="top-bar panel">
      <div>
        <p class="eyebrow">AGRI GATEWAY DEMO</p>
        <h1 class="title">Operations + Traceability Console</h1>
      </div>
      <nav class="main-nav">
        <RouterLink to="/operations" class="nav-link">Operations</RouterLink>
        <RouterLink to="/traceability" class="nav-link">Traceability</RouterLink>
      </nav>
    </header>

    <main>
      <RouterView />
    </main>
  </div>
</template>

<style scoped>
.app-shell {
  max-width: 1080px;
  margin: 0 auto;
  padding: 1.75rem 1rem 2.5rem;
}

.top-bar {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 1.2rem;
}

.eyebrow {
  margin: 0;
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0.16em;
  color: var(--ink-muted);
}

.title {
  margin: 0.3rem 0 0;
  font-size: clamp(1.2rem, 2vw, 1.6rem);
}

.main-nav {
  display: flex;
  align-items: center;
  gap: 0.65rem;
}

.nav-link {
  padding: 0.45rem 0.8rem;
  border-radius: 999px;
  border: 1px solid var(--line-soft);
  color: var(--ink-muted);
  text-decoration: none;
  transition: all 180ms ease;
}

.nav-link.router-link-active {
  border-color: var(--line-strong);
  background: rgba(32, 57, 84, 0.85);
  color: var(--ink);
}
</style>
