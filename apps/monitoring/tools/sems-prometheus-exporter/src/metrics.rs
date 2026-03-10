use std::fmt::Write;

/// Prometheus metric type.
#[derive(Clone, Copy)]
pub enum MetricType {
    Counter,
    Gauge,
}

impl MetricType {
    fn as_str(self) -> &'static str {
        match self {
            MetricType::Counter => "counter",
            MetricType::Gauge => "gauge",
        }
    }
}

/// A single Prometheus metric definition.
pub struct Metric<'a> {
    pub name: &'a str,
    pub help: &'a str,
    pub mtype: MetricType,
}

impl<'a> Metric<'a> {
    pub fn new(name: &'a str, help: &'a str, mtype: MetricType) -> Self {
        Self { name, help, mtype }
    }

    /// Render this metric with an i64 value into the output buffer.
    pub fn render(&self, value: i64, out: &mut String) {
        writeln!(out, "# HELP {} {}", self.name, self.help).unwrap();
        writeln!(out, "# TYPE {} {}", self.name, self.mtype.as_str()).unwrap();
        writeln!(out, "{} {}", self.name, value).unwrap();
    }

    /// Render this metric with labels and an i64 value.
    pub fn render_labeled(&self, labels: &str, value: i64, out: &mut String) {
        writeln!(out, "{}{} {}", self.name, labels, value).unwrap();
    }

    /// Render the HELP and TYPE header lines (for multi-sample metrics).
    pub fn render_header(&self, out: &mut String) {
        writeln!(out, "# HELP {} {}", self.name, self.help).unwrap();
        writeln!(out, "# TYPE {} {}", self.name, self.mtype.as_str()).unwrap();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn render_gauge() {
        let mut out = String::new();
        Metric::new("test_metric", "A test metric", MetricType::Gauge).render(42, &mut out);
        assert_eq!(
            out,
            "# HELP test_metric A test metric\n# TYPE test_metric gauge\ntest_metric 42\n"
        );
    }

    #[test]
    fn render_counter() {
        let mut out = String::new();
        Metric::new("test_total", "A test counter", MetricType::Counter).render(100, &mut out);
        assert_eq!(
            out,
            "# HELP test_total A test counter\n# TYPE test_total counter\ntest_total 100\n"
        );
    }

    #[test]
    fn render_labeled() {
        let mut out = String::new();
        let m = Metric::new("sems_count", "help", MetricType::Gauge);
        m.render_header(&mut out);
        m.render_labeled("{name=\"foo\"}", 10, &mut out);
        m.render_labeled("{name=\"bar\"}", 20, &mut out);
        assert!(out.contains("# HELP sems_count help\n"));
        assert!(out.contains("# TYPE sems_count gauge\n"));
        assert!(out.contains("sems_count{name=\"foo\"} 10\n"));
        assert!(out.contains("sems_count{name=\"bar\"} 20\n"));
    }

    #[test]
    fn render_zero() {
        let mut out = String::new();
        Metric::new("zero_metric", "zero", MetricType::Gauge).render(0, &mut out);
        assert!(out.contains("zero_metric 0\n"));
    }

    #[test]
    fn render_negative() {
        let mut out = String::new();
        Metric::new("neg_metric", "negative", MetricType::Gauge).render(-5, &mut out);
        assert!(out.contains("neg_metric -5\n"));
    }
}
