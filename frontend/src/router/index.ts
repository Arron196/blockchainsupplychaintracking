import { createRouter, createWebHistory } from "vue-router";

import AppShell from "@/layouts/AppShell.vue";
import OperationsDashboardPage from "@/pages/operations/OperationsDashboardPage.vue";
import TraceabilityPage from "@/pages/traceability/TraceabilityPage.vue";

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: "/",
      component: AppShell,
      children: [
        {
          path: "",
          redirect: { name: "operations-dashboard" }
        },
        {
          path: "operations",
          name: "operations-dashboard",
          component: OperationsDashboardPage
        },
        {
          path: "traceability",
          name: "traceability-view",
          component: TraceabilityPage
        }
      ]
    }
  ]
});

export default router;
