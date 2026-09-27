import {
  Button,
  Checkbox,
  type ComponentFn,
  Entry,
  Flex,
  padding,
  ref,
  runJSX,
  Sizing,
  Slider,
  Text,
} from "../mod.ts";

const name = ref("");
const subscribed = ref(false);
const volume = ref(0.5);
let clicks = 0;

const Greeting: ComponentFn = () => (
  <Text>Hello, {name.value || "stranger"}!</Text>
);

function App() {
  return (
    <Flex
      direction="vertical"
      gap={12}
      padding={padding(20)}
      width={Sizing.grow()}
    >
      <Text bold>n8v basic example (JSX)</Text>

      <Button
        style="primary"
        onClick={() => {
          clicks++;
          console.log("clicked", clicks, "times");
        }}
      >
        Click me
      </Button>

      <Entry value={name} placeholder="Your name" />
      <Checkbox checked={subscribed}>Subscribe to updates</Checkbox>
      <Slider value={volume} min={0} max={1} />

      <Greeting />
    </Flex>
  );
}

runJSX(480, 360, "n8v basic example (JSX edition :P)", () => <App />);
