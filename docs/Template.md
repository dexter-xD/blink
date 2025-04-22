# Template Engine Documentation

This document provides a comprehensive guide to using the C-based template engine for dynamic HTML content generation.

## Table of Contents

1. [Introduction](#introduction)
2. [Basic Template Syntax](#basic-template-syntax)
3. [Template Variables](#template-variables)
4. [Conditional Logic](#conditional-logic)
5. [Loops](#loops)
6. [Conditional Loops](#conditional-loops)
7. [Nested Elements](#nested-elements)
8. [Best Practices](#best-practices)

## Introduction

The template engine allows for dynamic HTML generation by combining static HTML with dynamic content. It supports:

- Variable replacement
- Conditional blocks (if/else)
- Loops over items
- Conditional loops with filtering
- Part extraction from delimited values

## Basic Template Syntax

### Variable Placeholders

Variables are referenced using double curly braces:

```html
<h1>Hello, {{username}}!</h1>
```

### Conditionals

Conditional blocks use the following syntax:

```html
{% if condition %}
  <!-- Content if condition is true -->
{% else %}
  <!-- Content if condition is false -->
{% endif %}
```

### Loops

Loops use the following syntax:

```html
{% for item in items %}
  <!-- Content repeated for each item -->
{% endfor %}
```

### Conditional Loops

Loops with conditions use the following syntax:

```html
{% for item in items if item.1 == "value" %}
  <!-- Content only for items that match the condition -->
{% endfor %}
```

## Template Variables

### Defining Variables

Variables can be defined in HTML comments:

```html
<!-- template:var username="John" is_admin="1" page_title="Dashboard" -->
```

### Using Variables

Variables are accessed using double curly braces:

```html
<title>{{page_title}}</title>
<h1>Welcome, {{username}}!</h1>
```

### Multi-part Variables

Variables can contain delimited values (using the pipe character):

```html
<!-- template:var product="Laptop|Electronics|999.99" -->
```

**Note**: Accessing parts of regular variables directly (like `{{product.0}}`) is not supported. Use loops for part extraction.

## Conditional Logic

### Basic Conditions

```html
{% if is_admin %}
  <a href="/admin">Admin Panel</a>
{% endif %}
```

### If/Else Blocks

```html
{% if is_logged_in %}
  <p>Welcome back, {{username}}!</p>
{% else %}
  <p>Please <a href="/login">log in</a> to continue.</p>
{% endif %}
```

### Condition Values

The engine treats the following values as "true":
- "1", "true", "yes", "y", "on"
- Any non-empty string not explicitly false

The following values are treated as "false":
- "0", "false", "no", "n", "off"
- Empty strings

## Loops

### Defining Loop Items

Loop items are defined in HTML comments:

```html
<!-- template:items "Item 1" "Item 2" "Item 3" "Item 4" "Item 5" -->
```

### Basic Loop

```html
<ul>
  {% for item in items %}
    <li>{{item}}</li>
  {% endfor %}
</ul>
```

### Multi-part Items

Items can contain parts separated by the pipe character:

```html
<!-- template:items "Apple|fruit|red|1.99" "Banana|fruit|yellow|0.99" "Carrot|vegetable|orange|1.49" -->
```

Access parts using dot notation:

```html
<ul>
  {% for item in items %}
    <li>{{item.0}} - {{item.1}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

## Conditional Loops

### Filtering by Exact Match

```html
<ul>
  {% for item in items if item.1 == "fruit" %}
    <li>{{item.0}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

### Filtering by Inequality

```html
<ul>
  {% for item in items if item.1 != "vegetable" %}
    <li>{{item.0}} - ${{item.3}}</li>
  {% endfor %}
</ul>
```

### Multiple Conditions

Currently only single conditions are supported. For complex filtering, pre-filter your data.

## Nested Elements

The template engine supports nesting conditional blocks and loops.

### Nested Conditionals

```html
{% if is_logged_in %}
  {% if is_admin %}
    <p>Admin view</p>
  {% else %}
    <p>User view</p>
  {% endif %}
{% endif %}
```

### Loops within Conditionals

```html
{% if has_items %}
  <ul>
    {% for item in items %}
      <li>{{item}}</li>
    {% endfor %}
  </ul>
{% else %}
  <p>No items found.</p>
{% endif %}
```

## Best Practices

1. **Keep templates simple**: Separate presentation from complex logic
2. **Use meaningful variable names**: Clear names make templates more maintainable
3. **Limit nesting depth**: Excessive nesting makes templates hard to read
4. **Add comments**: Document complex templates with HTML comments
5. **Structure multi-part data consistently**: Use consistent field order in pipe-delimited values
6. **Test templates with minimal data first**: Validate template structure before using complex data
7. **Consider caching**: For production use, cache processed templates to improve performance
8. **Validate user input**: Sanitize any user-supplied values used in templates

## Implementation Details

Under the hood, the template processor performs multiple passes over the content:

1. Replace variables with their values
2. Process if/else conditions
3. Process loops (with or without conditions)

Each pass transforms the content progressively until the final output is generated. 