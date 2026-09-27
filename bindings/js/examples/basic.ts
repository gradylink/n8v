import * as n8v from "../mod.ts";

const name = n8v.ref("");
const subscribed = n8v.ref(false);
const volume = n8v.ref(0.5);
let clicks = 0;

n8v.run(480, 360, "n8v basic example", () => {
  n8v.flex(
    {
      direction: "vertical",
      gap: 12,
      padding: n8v.padding(20),
      width: n8v.Sizing.grow(),
    },
    () => {
      n8v.text("n8v basic example", { bold: true });

      n8v.button("Click me", {
        style: "primary",
        onClick: () => {
          clicks++;
          console.log("clicked", clicks, "times");
        },
      });

      n8v.entry({ value: name, placeholder: "Your name" });
      n8v.checkbox("Subscribe to updates", { checked: subscribed });
      n8v.slider({ value: volume, min: 0, max: 1 });

      n8v.text(`Hello, ${name.value || "stranger"}!`);
    },
  );
});
